/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <map>
#include <vector>
#include "esp_log.h"
#include "esp_console.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "protocol_examples_common.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "servo.hpp"
#include "app_spiffs.hpp"
#include "app_qwen_vl.hpp"
#include "app_cam.h"
#include "app_manager.hpp"
#include "ui.h"

Servo *servo = nullptr;

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    std::map<int, ServoConfig> servo_configs = {
        {1, {900, 3100, 0, 180, false}},
        {2, {900, 3100, 0, 180, true}},
        {3, {900, 3100, 0, 180, true}},
        {4, {900, 3100, 0, 180, true}},
        {5, {380, 3700, 0, 270, false}},
        {6, {900, 3100, 0, 180, false}},
    };
    servo = new Servo(UART_NUM_1, 24, 25, servo_configs);

    app_uvc_init();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    app_uvc_control_dev_by_index(0, true, 4);  /*!< 640x480 30fps */

    bsp_display_start();
    ESP_ERROR_CHECK(bsp_display_backlight_on());

    bsp_display_lock(0);
    ui_init();
    bsp_display_unlock();

    auto manager = new Manager(servo);
    manager->run();
}
