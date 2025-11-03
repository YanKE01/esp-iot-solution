/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "kinematic.h"
#include "app_lcd.h"
#include "dm_motor.h"

static const char *TAG = "main";

extern "C" void app_main(void)
{
    damiao::Motor_Control* motor_control = damiao::Motor_Control::getInstance(GPIO_NUM_24, GPIO_NUM_25);
    esp_err_t ret = motor_control->init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Motor control initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Motor control initialized successfully");

    ESP_Brookesia_Phone* phone = app_lcd_init();
    assert(phone != nullptr);

    while (1) {
        uint16_t free_sram_size_kb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
        uint16_t total_sram_size_kb = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024;
        uint16_t free_psram_size_kb = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;
        uint16_t total_psram_size_kb = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024;

        phone->lockLv();
        // Update memory label on "Recents Screen"
        if (!phone->getHome().getRecentsScreen()->setMemoryLabel(free_sram_size_kb, total_sram_size_kb, free_psram_size_kb, total_psram_size_kb)) {
            ESP_LOGE(TAG, "Set memory label failed");
        }

        phone->unlockLv();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
