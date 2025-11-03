/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "dm_motor.h"

class PantheraCtrl: public ESP_Brookesia_PhoneApp {
public:
    PantheraCtrl(damiao::Motor_Control* motor_control);
    ~PantheraCtrl();

    bool init(void) override;
    bool run(void) override;
    bool pause(void) override;
    bool resume(void) override;
    bool back(void) override;
    bool close(void) override;
private:
    static void ui_EnableSwitch_event_handler(lv_event_t *e);
    static void angle_update_timer_cb(lv_timer_t *timer);
    static void decrease_event_handler(lv_event_t *e);
    static void increase_event_handler(lv_event_t *e);
    void handle_joint_control(uint32_t master_id, bool increase);
    void update_joint_angles();
    lv_timer_t *angle_update_timer_;
    damiao::Motor_Control* motor_control_;

};
