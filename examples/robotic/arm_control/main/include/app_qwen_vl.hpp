/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "esp_http_client.h"

class QwenVL {
public:
    QwenVL();
    void run(const char* jpg, size_t jpg_size);
private:
    static esp_err_t qwen_event_handler(esp_http_client_event_t* evt);
    static constexpr const char *qwen_vl_url = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
    esp_http_client_handle_t m_client;
    esp_http_client_config_t m_config = {};
    size_t m_img_base64_size;
    char *m_img_base64_buf;
};
