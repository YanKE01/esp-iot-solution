#include <array>
#include "dmbot.hpp"
#include "esp_log.h"

namespace damiao {

#define P_MIN -12.5f
#define P_MAX 12.5f
#define V_MIN -30.0f
#define V_MAX 30.0f
#define KP_MIN 0.0f
#define KP_MAX 500.0f
#define KD_MIN 0.0f
#define KD_MAX 5.0f
#define T_MIN -10.0f
#define T_MAX 10.0f

#define MIT_MODE    0x000
#define POS_MODE    0x100
#define SPEED_MODE  0x200

static const char *TAG = "DM_Bot";

// Static member variable definitions
bool Motor_Control::initialized = false;
TaskHandle_t Motor_Control::data_process_task_handle = NULL;
QueueHandle_t Motor_Control::can_data_queue = NULL;

Limit_param_t limit_param[Num_Of_Motor] = {
    {12.5, 30, 10 }, // DM4310
    {12.5, 50, 10 }, // DM4310_48V
    {12.5, 8, 28 },  // DM4340
    {12.5, 10, 28 }, // DM4340_48V
    {12.5, 45, 20 }, // DM6006
    {12.5, 45, 40 }, // DM8006
    {12.5, 45, 54 }, // DM8009
    {12.5, 25,  200}, // DM10010L
    {12.5, 20, 200}, // DM10010
    {12.5, 280, 1},  // DMH3510
    {12.5, 45, 10},  // DMH6215
    {12.5, 45, 10}   // DMG6220
};

Motor::Motor(DM_Motor_Type Motor_Type, uint32_t Slave_id, uint32_t Master_id)
{
    this->Slave_id = Slave_id;
    this->Master_id = Master_id;
    this->Motor_Type = Motor_Type;
    this->limit_param = ::damiao::limit_param[Motor_Type];
}

uint32_t Motor::GetSlaveId() const
{
    return this->Slave_id;
}

uint32_t Motor::GetMasterId() const
{
    return this->Master_id;
}

DM_Motor_Type Motor::GetMotorType() const
{
    return this->Motor_Type;
}

bool Motor_Control::switch_control_mode(const Motor &motor, Control_Mode control_mode)
{
    uint8_t write_data[4] = {(uint8_t)control_mode, 0x00, 0x00, 0x00};
    write_motor_param(motor, 10, write_data);
    vTaskDelay(pdMS_TO_TICKS(100));
    return true;
}

void Motor_Control::add_motor(Motor *motor)
{
    if (motor->GetMasterId() != 0x00) {
        motor_maps.insert({motor->GetMasterId(), motor});
    }
}

Motor_Control &Motor_Control::getInstance()
{
    static Motor_Control instance;
    return instance;
}

void Motor_Control::initialize(gpio_num_t tx, gpio_num_t rx)
{
    if (initialized) {
        ESP_LOGW(TAG, "Motor_Control already initialized");
        return;
    }

    // Additional check: ensure queue and task handles are NULL
    if (can_data_queue != NULL || data_process_task_handle != NULL) {
        ESP_LOGW(TAG, "Queue or task already exists, skipping creation");
        return;
    }

    // Create CAN data queue
    can_data_queue = xQueueCreate(20, sizeof(twai_frame_t));
    if (can_data_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create CAN data queue");
        return;
    }

    // Create data processing task
    Motor_Control &instance = getInstance();
    BaseType_t ret = xTaskCreate(can_data_process_task, "can_data_process", 4096, &instance, 5, &data_process_task_handle);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CAN data process task");
        vQueueDelete(can_data_queue);
        can_data_queue = NULL;
        return;
    }

    instance.init(tx, rx);
    initialized = true;
    ESP_LOGI(TAG, "Motor_Control initialized successfully");
}

void Motor_Control::init(gpio_num_t tx, gpio_num_t rx)
{

    twai_onchip_node_config_t node_config = {};

    node_config.io_cfg.tx = tx;
    node_config.io_cfg.rx = rx;
    node_config.bit_timing.bitrate = 1000000;
    node_config.fail_retry_cnt = 10;
    node_config.tx_queue_depth = 10;

    if (twai_new_node_onchip(&node_config, &twai_node) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create TWAI node");
        return;
    }

    // Register event callbacks
    twai_event_callbacks_t callbacks = {};
    callbacks.on_rx_done = twai_rx_done_cb;
    twai_node_register_event_callbacks(twai_node, &callbacks, this);

    if (twai_node_enable(twai_node) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable TWAI node");
        return;
    }
}

Motor_Control::~Motor_Control()
{
    if (twai_node_disable(twai_node) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable TWAI node");
    }

    if (twai_node_delete(twai_node) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete TWAI node");
    }

    twai_node = NULL;
}

void Motor_Control::control_cmd(uint32_t id, uint8_t cmd)
{
    if (twai_node == NULL) {
        ESP_LOGE(TAG, "TWAI node is not initialized");
        return;
    }
    uint8_t data_buf[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, cmd};

    twai_frame_t tx_frame = {};
    tx_frame.header.id = id;
    tx_frame.buffer = (uint8_t *) data_buf;
    tx_frame.buffer_len = sizeof(data_buf);

    if (twai_node_transmit(twai_node, &tx_frame, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit TWAI frame");
    }
}

void Motor_Control::write_motor_param(const Motor &motor, uint8_t rid, const uint8_t data[4])
{
    uint32_t id = motor.GetSlaveId();
    uint8_t can_low = id & 0xff;
    uint8_t can_high = (id >> 8) & 0xff;
    uint8_t data_buf[8] = {can_low, can_high, 0x55, rid, 0x00, 0x00, 0x00, 0x00};
    for (int i = 0; i < 4; i++) {
        data_buf[i + 4] = data[i];
    }

    twai_frame_t tx_frame = {};
    tx_frame.header.id = id;
    tx_frame.buffer = (uint8_t *) data_buf;
    tx_frame.buffer_len = sizeof(data_buf);

    if (twai_node_transmit(twai_node, &tx_frame, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit TWAI frame");
    }
}

void Motor_Control::refersh_motor_status(const Motor &motor)
{
    uint32_t id = 0x7FF;
    uint8_t can_low = motor.GetSlaveId() & 0xff; // id low 8 bit
    uint8_t can_high = (motor.GetSlaveId() >> 8) & 0xff; //id high 8 bit

    uint8_t data_buf[8] = {can_low, can_high, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00};

    twai_frame_t tx_frame = {};
    tx_frame.header.id = id;
    tx_frame.buffer = (uint8_t *) data_buf;
    tx_frame.buffer_len = sizeof(data_buf);

    if (twai_node_transmit(twai_node, &tx_frame, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit TWAI frame");
    }

    vTaskDelay(pdMS_TO_TICKS(10));

}

void Motor_Control::enable(const Motor &motor)
{
    control_cmd(motor.GetSlaveId(), 0xFC);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void Motor_Control::disable(const Motor &motor)
{
    control_cmd(motor.GetSlaveId(), 0xFD);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void Motor_Control::set_zero_pos(const Motor &motor)
{
    control_cmd(motor.GetSlaveId(), 0xFE);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void Motor_Control::control_pos_vel(const Motor &motor, float position, float velocity)
{
    uint16_t id;
    uint8_t *pbuf, *vbuf;
    uint8_t data[8];

    id = motor.GetSlaveId() + POS_MODE;
    pbuf = (uint8_t *)&position;
    vbuf = (uint8_t *)&velocity;
    data[0] = *pbuf;
    data[1] = *(pbuf + 1);
    data[2] = *(pbuf + 2);
    data[3] = *(pbuf + 3);
    data[4] = *vbuf;
    data[5] = *(vbuf + 1);
    data[6] = *(vbuf + 2);
    data[7] = *(vbuf + 3);

    twai_frame_t tx_frame = {};
    tx_frame.header.id = id;
    tx_frame.buffer = (uint8_t *) data;
    tx_frame.buffer_len = sizeof(data);

    if (twai_node_transmit(twai_node, &tx_frame, pdMS_TO_TICKS(100)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transmit TWAI frame");
    }

    vTaskDelay(pdMS_TO_TICKS(10));
}

bool IRAM_ATTR Motor_Control::twai_rx_done_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    // Use static buffer to ensure data remains valid after callback returns
    static uint8_t recv_buff[TWAI_FRAME_MAX_LEN] = {0};
    twai_frame_t rx_frame = {};
    rx_frame.buffer = recv_buff;
    rx_frame.buffer_len = sizeof(recv_buff);

    // Receive data from ISR
    if (twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK) {
        // Send data to queue (from ISR) - copies the structure and buffer pointer
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(can_data_queue, &rx_frame, &xHigherPriorityTaskWoken);

        // Trigger task switch if needed
        if (xHigherPriorityTaskWoken) {
            return true;
        }
    }

    return false;
}

int Motor_Control::float_to_unit(float x_float, float x_min, float x_max, int bits)
{
    float span = x_max - x_min;
    float offset = x_min;
    return (int)((x_float - offset) * ((float)((1 << bits) - 1)) / span);
}

float Motor_Control::unit_to_float(int x_unit, float x_min, float x_max, int bits)
{
    float span = x_max - x_min;
    float offset = x_min;
    return ((float)x_unit) * span / ((float)((1 << bits) - 1)) + offset;
}

void Motor_Control::can_data_process_task(void *pvParameters)
{
    Motor_Control *motor_control = static_cast<Motor_Control*>(pvParameters);
    twai_frame_t rx_frame;

    ESP_LOGI(TAG, "CAN data process task started");

    while (1) {
        // Receive CAN data from queue
        if (xQueueReceive(can_data_queue, &rx_frame, portMAX_DELAY) == pdTRUE) {
            if (rx_frame.buffer_len == TWAI_FRAME_MAX_LEN) {
                uint32_t master_id = rx_frame.header.id;

                // Find motor by master ID
                auto it = motor_control->motor_maps.find(master_id);
                if (it != motor_control->motor_maps.end()) {
                    Motor *motor = it->second;
                    ESP_LOGD(TAG, "Received data for motor with master ID: 0x%08X", master_id);

                    // Parse CAN data and update motor feedback parameters
                    motor->motor_fb_param.state = rx_frame.buffer[0] >> 4;

                    // Check for error states and log them
                    switch (motor->motor_fb_param.state) {
                    case 8:
                        ESP_LOGE(TAG, "Motor 0x%08X: Over voltage error", master_id);
                        break;
                    case 9:
                        ESP_LOGE(TAG, "Motor 0x%08X: Under voltage error", master_id);
                        break;
                    case 0xA:
                        ESP_LOGE(TAG, "Motor 0x%08X: Over current error", master_id);
                        break;
                    case 0xB:
                        ESP_LOGE(TAG, "Motor 0x%08X: MOS over temperature error", master_id);
                        break;
                    case 0xC:
                        ESP_LOGE(TAG, "Motor 0x%08X: Motor coil over temperature error", master_id);
                        break;
                    case 0xD:
                        ESP_LOGE(TAG, "Motor 0x%08X: Communication lost error", master_id);
                        break;
                    case 0xE:
                        ESP_LOGE(TAG, "Motor 0x%08X: Overload error", master_id);
                        break;
                    default:
                        // Normal state, no error
                        break;
                    }

                    int position = (rx_frame.buffer[1] << 8) | (rx_frame.buffer[2]);
                    motor->motor_fb_param.position = motor_control->unit_to_float(position, P_MIN, P_MAX, 16);
                    int velocity = (rx_frame.buffer[3] << 4) | (rx_frame.buffer[4] >> 4);
                    motor->motor_fb_param.velocity = motor_control->unit_to_float(velocity, V_MIN, V_MAX, 12);
                    int torque = ((rx_frame.buffer[4] & 0xF) << 8) | (rx_frame.buffer[5]);
                    motor->motor_fb_param.torque = motor_control->unit_to_float(torque, T_MIN, T_MAX, 12);

                    motor->motor_fb_param.temper_mos = (float)rx_frame.buffer[6];
                    motor->motor_fb_param.temper_rotor = (float)rx_frame.buffer[7];

                } else {
                    ESP_LOGW(TAG, "Unknown motor master ID: 0x%08X", master_id);
                }
            }
        }
    }
}

};
