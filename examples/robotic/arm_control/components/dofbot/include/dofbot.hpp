/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <vector>
#include "driver/uart.h"

struct ServoConfig {
    int min_pwm;
    int max_pwm;
    int min_angle;
    int max_angle;
    bool reversed;
};

class Dofbot {
public:
    Dofbot(uart_port_t port, int tx_pin, int rx_pin, std::map<int, ServoConfig> servo_config, int baudrate = 115200);
    void write(uint8_t* data, size_t size);
    esp_err_t control(uint8_t id, int angle, uint16_t time);
    esp_err_t control(std::vector<int> angle, uint16_t time);
    esp_err_t move(double x, double y, double z);
    double sin_deg(double degree);
    double cos_deg(double degree);
    double joint_deg_convert(int joint_id, double degree);
    bool joint_deg_is_valid(int joint_id, double degree);
    bool forward_kinematics(double theta1, double theta2, double theta3, double theta4, double *x, double *y, double *z);
private:
    static void rx_task(void* arg);
    uart_port_t m_port;
    uint8_t *m_rx_data;
    const size_t m_rx_data_size = 1024;
    std::map<int, ServoConfig> m_servo_configs;
};
