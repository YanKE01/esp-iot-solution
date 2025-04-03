/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string>
#include <sstream>
#include <iostream>
#include "esp_log.h"
#include "mbedtls/base64.h"
#include "app_qwen_vl.hpp"

static constexpr const char *TAG = "QwenVL";

QwenVL::QwenVL()
{
    m_config.method = HTTP_METHOD_POST;
    m_config.buffer_size = 20 * 1024;
    m_config.url = qwen_vl_url;
    m_config.timeout_ms = 25000;
    m_config.event_handler = qwen_event_handler;
    m_config.user_data = this;
}

esp_err_t QwenVL::qwen_event_handler(esp_http_client_event_t* evt)
{
    QwenVL* self = static_cast<QwenVL*>(evt->user_data);

    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        ESP_LOGI(TAG, "%.*s", evt->data_len, (char *)evt->data);
    }
    return ESP_OK;
}

void QwenVL::run(const char* jpg, size_t jpg_size)
{
    mbedtls_base64_encode(nullptr, 0, &m_img_base64_size, (const unsigned char *)jpg, jpg_size);
    ESP_LOGI(TAG, "Image size after base64:%zu", m_img_base64_size);

    m_img_base64_buf = (char*)heap_caps_calloc(1, m_img_base64_size + 1, MALLOC_CAP_SPIRAM);

    if (m_img_base64_buf == NULL) {
        ESP_LOGI(TAG, "Memory image bash64 allocation failed");
        return;
    }

    mbedtls_base64_encode((unsigned char *)m_img_base64_buf, m_img_base64_size + 1, &m_img_base64_size, (const unsigned char *)jpg, jpg_size);
    std::string base64_str(m_img_base64_buf, m_img_base64_size);

    std::ostringstream oss;
    oss << R"({
        "model": "qwen2.5-vl-32b-instruct",
        "messages": [
        {
            "role": "user",
            "content": [
                {
                    "type": "image_url",
                    "image_url": {
                        "url": "data:image/jpg;base64,)" << base64_str << R"("
                    }
                },
                {
                    "type": "text",
                    "text": "Detect the objects in the image, identify their categories, and return their locations in the form of coordinates. The output format should be like {\"objects\": [{\"category\": \"object_category\", \"bbox_2d\": [x1, y1, x2, y2]}]}"
                }
            ]
        }
        ]
        })";

    std::string json_data = oss.str();
    m_client = esp_http_client_init(&m_config);
    esp_http_client_set_method(m_client, HTTP_METHOD_POST);
    esp_http_client_set_header(m_client, "Content-Type", "application/json");
    esp_http_client_set_header(m_client, "Authorization", CONFIG_EXAMPLE_QWEN_API_KEY);
    esp_http_client_set_post_field(m_client, json_data.c_str(), json_data.length());
    esp_err_t err = esp_http_client_perform(m_client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d", esp_http_client_get_status_code(m_client), (int)esp_http_client_get_content_length(m_client));
    } else {
        ESP_LOGI(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(m_client);

    free(m_img_base64_buf);
}
