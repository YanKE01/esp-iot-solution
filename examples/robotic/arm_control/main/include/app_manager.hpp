/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "app_qwen_vl.hpp"
#include "driver/jpeg_decode.h"
#include "lvgl.h"

class Manager {
public:
    Manager();
    ~Manager();
    void run();
    enum class manager_event_t { RECOGNIZE = 1 << 0, CLEAR = 1 << 1};
private:
    static void lcd_refresh_task(void *arg);
    static void event_handle_task(void *arg);
    static void lvgl_btn_event_handler(lv_event_t *event);
    static TaskHandle_t m_task_handle;

    std::unique_ptr<QwenVL> m_qwen_vl;
    SemaphoreHandle_t m_mutex;
    jpeg_decoder_handle_t m_jpeg_dec_handle;
    jpeg_decode_cfg_t m_jpeg_dec_config;
    size_t m_jpeg_dec_output_buf_alloced_size = 0;
    uint32_t m_jpeg_decoded_size = 0;
    uint8_t *m_jpeg_dec_output_buf = nullptr;
};

extern Manager* g_manager_instance;
