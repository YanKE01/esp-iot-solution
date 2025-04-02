/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "servo.hpp"

Servo::Servo(uart_port_t port, int tx_pin, int rx_pin, std::map<int, ServoConfig> servo_config, int baudrate)
{
    uart_config_t uart_config = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(port, m_rx_data_size * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(port, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    m_port = port;
    m_rx_data = new uint8_t[m_rx_data_size];
    m_servo_configs = servo_config;

    xTaskCreate(rx_task, "uart_rx_task", 4096, this, 10, NULL);
}

void Servo::rx_task(void* arg)
{
    Servo *self = (Servo *)arg;

    while (1) {
        std::fill(self->m_rx_data, self->m_rx_data + self->m_rx_data_size, 0);
        int len = uart_read_bytes(self->m_port, self->m_rx_data, (self->m_rx_data_size - 1), 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                printf("%02X ", self->m_rx_data[i]);
            }
            printf("\n");
        }

    }
}

void Servo::write(uint8_t* data, size_t size)
{
    uart_write_bytes(m_port, (const char *)data, size);
}

esp_err_t Servo::control(uint8_t id, int angle, uint16_t time)
{
    auto it = m_servo_configs.find(id);
    if (it == m_servo_configs.end()) {
        return ESP_ERR_INVALID_ARG;
    }

    const ServoConfig &config = it->second;
    if (angle < config.min_angle || angle > config.max_angle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (config.reversed) {
        angle = config.max_angle - angle;
    }

    int real_pos = int((config.max_pwm - config.min_pwm) * (angle - config.min_angle) / (config.max_angle - config.min_angle) + config.min_pwm);

    uint8_t data[] = {0xFF, 0xFF, id, 0x07, 0x03, 0x2A,
                      static_cast<uint8_t>((real_pos >> 8) & 0xFF),
                      static_cast<uint8_t>((real_pos) & 0xFF),
                      static_cast<uint8_t>((time >> 8) & 0xFF),
                      static_cast<uint8_t>(time & 0xFF),
                      0x00
                     };

    uint8_t check_sum = 0;
    for (int i = 2; i < sizeof(data) - 1; i++) {
        check_sum += data[i];
    }
    data[sizeof(data) - 1] = ~check_sum;

    write(data, sizeof(data));
    return ESP_OK;
}

esp_err_t Servo::control(std::vector<int> angle, uint16_t time)
{
    if (angle.size() > m_servo_configs.size()) {
        return ESP_ERR_INVALID_ARG;
    }

    std::vector<uint8_t> params = {0x2A, 0x04};

    printf("angle size: %d\n", angle.size());

    for (size_t i = 0; i < angle.size(); i++) {
        uint8_t id = static_cast<uint8_t>(i + 1);            /*!< The ID sequence must start at 1 */

        // Check id
        auto it = m_servo_configs.find(id);
        if (it == m_servo_configs.end()) {
            return ESP_ERR_INVALID_ARG;
        }

        const ServoConfig &config = it->second;
        int current_angle = angle[i];
        if (current_angle < config.min_angle || current_angle > config.max_angle) {
            return ESP_ERR_INVALID_ARG;
        }

        if (config.reversed) {
            current_angle = config.max_angle - current_angle;
        }

        int real_pos = static_cast<int>((config.max_pwm - config.min_pwm) * (current_angle - config.min_angle) / (config.max_angle - config.min_angle) + config.min_pwm);

        uint8_t param[] = {id,
                           static_cast<uint8_t>((real_pos >> 8) & 0xFF),
                           static_cast<uint8_t>((real_pos) & 0xFF),
                           static_cast<uint8_t>((time >> 8) & 0xFF),
                           static_cast<uint8_t>(time & 0xFF),
                          };
        params.insert(params.end(), std::begin(param), std::end(param));
    }

    std::vector<uint8_t> data = {0xFF, 0xFF, 0xFE};
    data.push_back(static_cast<uint8_t>(params.size() + 2)); /*!< The total data length is calculated as the sum of the ID, command type, and parameter length */
    data.push_back(0x83);                                    /*!< Command type */
    data.insert(data.end(), params.begin(), params.end());

    uint8_t check_sum = 0;
    for (int i = 2; i < data.size(); i++) {
        check_sum += data[i];
    }
    data.push_back(~check_sum);

    write(data.data(), data.size());
    return ESP_OK;
}
