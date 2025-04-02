/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <map>
#include <vector>
#include <argtable3/argtable3.h>
#include "servo.hpp"
#include "esp_log.h"
#include "esp_console.h"
#include "app_wifi.hpp"
#include "app_spiffs.hpp"
#include "app_qwen_vl.hpp"

Servo *servo = nullptr;
QwenVL *qwen_vl = nullptr;
static const char *TAG = "app_main";

extern "C" void app_main(void)
{
    app_wifi_init();
    app_spiffs_init("/spiffs");

    std::map<int, ServoConfig> servo_configs = {
        {1, {900, 3100, 0, 180, false}},
        {2, {900, 3100, 0, 180, true}},
        {3, {900, 3100, 0, 180, true}},
        {4, {900, 3100, 0, 180, true}},
        {5, {380, 3700, 0, 270, false}},
        {6, {900, 3100, 0, 180, false}},
    };
    servo = new Servo(UART_NUM_1, 24, 25, servo_configs);
    qwen_vl = new QwenVL();

    const char *filename = "/spiffs/test.jpg";
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        ESP_LOGI(TAG, "Open %s failed", filename);
        return;
    }

    fseek(f, 0, SEEK_END);
    size_t filesize = ftell(f);
    rewind(f);
    ESP_LOGI(TAG, "File size:%zu", filesize);

    char *file_buf = (char *)malloc(filesize + 1);
    if (file_buf == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        fclose(f);
        return;
    }

    if (fread(file_buf, 1, filesize, f) != filesize) {
        fprintf(stderr, "Failed to read the file.\n");
        fclose(f);
        free(file_buf);
        return;
    }

    qwen_vl->run(file_buf, filesize);

}
