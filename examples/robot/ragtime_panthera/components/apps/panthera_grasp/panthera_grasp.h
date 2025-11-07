/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/jpeg_decode.h"
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "color_detect.hpp"
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
    static void lcd_refresh_task(void* pvParameters);
    static void detect_task(void* pvParameters);

    // Queue message structure for passing image data to detect task
    struct detect_queue_msg_t {
        uint8_t* image_data;
        size_t image_size;
    };

    // Motor control related
    damiao::Motor_Control* motor_control_;

    // JPEG decoder related
    jpeg_decoder_handle_t jpeg_decoder_handle_;
    jpeg_decode_cfg_t jpeg_decode_cfg_;
    uint8_t* jpeg_decode_buffer_;
    size_t jpeg_decode_buffer_size_;
    uint32_t jpeg_decoded_size_;

    // Task related
    TaskHandle_t lcd_refresh_task_handle_;
    TaskHandle_t detect_task_handle_;

    // Queue related
    QueueHandle_t detect_queue_handle_;

    // Detection results related
    std::list<dl::detect::result_t> detect_results_;
    SemaphoreHandle_t detect_results_mutex_;

    // AI related
    ColorDetect* color_detect_;
};
