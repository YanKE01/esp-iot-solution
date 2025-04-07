/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "esp_http_client.h"
#include <map>
#include <string>

// Define a structure to store bounding box coordinates
struct BoundingBox {
    int x1;
    int y1;
    int x2;
    int y2;
};

typedef struct {
    char* data;
    size_t len;
} qwen_response_t;

// Structure to hold parsed object detection results
struct ObjectDetectionResult {
    std::map<std::string, BoundingBox> objects;
};

class QwenVL {
public:
    QwenVL();
    void run(const char* jpg, size_t jpg_size);
    void set_response_queue(QueueHandle_t queue);

    // Parse JSON response and return object detection results
    static ObjectDetectionResult parse_object_detection_json(const char* json_str);

private:
    static esp_err_t qwen_event_handler(esp_http_client_event_t* evt);
    static constexpr const char *qwen_vl_url = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
    esp_http_client_handle_t m_client;
    esp_http_client_config_t m_config = {};
    size_t m_img_base64_size;
    char *m_img_base64_buf;
    QueueHandle_t m_response_queue;
};
