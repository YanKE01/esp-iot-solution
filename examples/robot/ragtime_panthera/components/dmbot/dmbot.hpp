#pragma once

#include <unordered_map>
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace damiao {

enum DM_Motor_Type {
    DM4310,
    DM4310_48V,
    DM4340,
    DM4340_48V,
    DM6006,
    DM8006,
    DM8009,
    DM10010L,
    DM10010,
    DMH3510,
    DMH6215,
    DMG6220,
    Num_Of_Motor
};

enum Control_Mode {
    MIT_MODE = 1,
    POS_VEL_MODE = 2,
    VEL_MODE = 3,
    POS_FORCE_MODE = 4,
};

typedef struct {
    float Q_MAX;
    float DQ_MAX;
    float TAU_MAX;
} Limit_param_t;

typedef struct {
    int state;
    float position;
    float velocity;
    float torque;
    float temper_mos;
    float temper_rotor;
} motor_fb_param_t;

class Motor {
public:
    Motor(DM_Motor_Type Motor_Type, uint32_t Slave_id, uint32_t Master_id);
    uint32_t GetSlaveId() const;
    uint32_t GetMasterId() const;
    DM_Motor_Type GetMotorType() const;
    motor_fb_param_t motor_fb_param;
private:
    Limit_param_t limit_param;
    uint32_t Slave_id;
    uint32_t Master_id;
    DM_Motor_Type Motor_Type;
};

class Motor_Control {
public:
    static Motor_Control &getInstance();
    static void initialize(gpio_num_t tx, gpio_num_t rx);
    void enable(const Motor &motor);
    void disable(const Motor &motor);
    void add_motor(Motor *motor);
    void set_zero_pos(const Motor &motor);
    void control_pos_vel(const Motor &motor, float position, float velocity);
    bool switch_control_mode(const Motor &motor, Control_Mode control_mode);
    void refersh_motor_status(const Motor &motor);
    Motor_Control(const Motor_Control &) = delete;
    Motor_Control &operator=(const Motor_Control &) = delete;

private:
    Motor_Control() = default;
    ~Motor_Control();
    void init(gpio_num_t tx, gpio_num_t rx);
    void control_cmd(uint32_t id, uint8_t cmd);
    void write_motor_param(const Motor &motor, uint8_t rid, const uint8_t data[4]);
    static bool IRAM_ATTR twai_rx_done_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx);
    static void can_data_process_task(void *pvParameters);

    // Utility functions for data conversion
    int float_to_unit(float x_float, float x_min, float x_max, int bits);
    float unit_to_float(int x_unit, float x_min, float x_max, int bits);

    twai_node_handle_t twai_node = NULL;
    static bool initialized;
    static TaskHandle_t data_process_task_handle;
    static QueueHandle_t can_data_queue;
    std::unordered_map<uint32_t, Motor*> motor_maps;
};

};
