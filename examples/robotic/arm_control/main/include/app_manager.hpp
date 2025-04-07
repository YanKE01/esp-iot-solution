/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <map>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "app_qwen_vl.hpp"
#include "driver/jpeg_decode.h"
#include "lvgl.h"

// Define a structure to store click coordinates
struct ClickCoordinates {
    int x;
    int y;
};

class Manager {
public:
    Manager();
    ~Manager();
    void run();
    enum class manager_event_t { RECOGNIZE = 1 << 0, CLEAR = 1 << 1};
    enum class manager_status_t { NORMAL = 0, TUNING = 1};
private:
    static void lcd_refresh_task(void *arg);
    static void event_handle_task(void *arg);
    static void lvgl_btn_event_handler(lv_event_t *event);
    static void draw_container_click_handler(lv_event_t *event);
    static TaskHandle_t m_task_handle;
    QueueHandle_t m_response_queue;
    std::unique_ptr<QwenVL> m_qwen_vl;
    SemaphoreHandle_t m_mutex;
    jpeg_decoder_handle_t m_jpeg_dec_handle;
    jpeg_decode_cfg_t m_jpeg_dec_config;
    size_t m_jpeg_dec_output_buf_alloced_size = 0;
    uint32_t m_jpeg_decoded_size = 0;
    uint8_t *m_jpeg_dec_output_buf = nullptr;

    // Add a map to store objects and their coordinates
    std::map<std::string, BoundingBox> m_detected_objects;
    std::map<int, ClickCoordinates> m_click_coordinates;
    int m_click_coordinates_index = 0;

    // Draw container for UI elements
    lv_obj_t *m_draw_container = nullptr;
};

extern Manager* g_manager_instance;
