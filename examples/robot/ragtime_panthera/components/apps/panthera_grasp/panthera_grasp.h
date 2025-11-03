/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "dm_motor.h"

class PantheraGrasp: public ESP_Brookesia_PhoneApp {
public:
    PantheraGrasp(damiao::Motor_Control* motor_control);
    ~PantheraGrasp();

    bool init(void) override;
    bool run(void) override;
    bool pause(void) override;
    bool resume(void) override;
    bool back(void) override;
    bool close(void) override;
private:
    damiao::Motor_Control* motor_control_;

};
