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
#include "cJSON.h"

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
        ESP_LOGD(TAG, "%.*s", evt->data_len, (char *)evt->data);
        qwen_response_t msg;
        msg.len = evt->data_len;
        msg.data = (char*)heap_caps_calloc(1, msg.len + 1, MALLOC_CAP_SPIRAM);

        if (!msg.data) {
            ESP_LOGE(TAG, "Failed to malloc for response data");
            return ESP_ERR_NO_MEM;
        }

        memcpy(msg.data, evt->data, msg.len);
        msg.data[msg.len] = '\0';

        if (self->m_response_queue != nullptr) {
            xQueueSend(self->m_response_queue, &msg, portMAX_DELAY);
        }
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
                    "text": "Detect the main objects on the white paper in the image, identify their categories with very short names (1-2 words maximum), and return their locations in the form of coordinates. Return ONLY a raw JSON object without any markdown formatting, code blocks, or additional text. The JSON should be in this exact format: {\"objects\": [{\"category\": \"object_category\", \"bbox_2d\": [x1, y1, x2, y2]}]}."
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

void QwenVL::set_response_queue(QueueHandle_t queue)
{
    m_response_queue = queue;
}

ObjectDetectionResult QwenVL::parse_object_detection_json(const char* json_str)
{
    ObjectDetectionResult result;

    // Parse the JSON response
    cJSON *root = cJSON_Parse(json_str);
    if (root != NULL) {
        // Navigate to the content field which contains the actual JSON with object detection results
        cJSON *choices = cJSON_GetObjectItem(root, "choices");
        if (choices != NULL && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
            cJSON *first_choice = cJSON_GetArrayItem(choices, 0);
            if (first_choice != NULL) {
                cJSON *message = cJSON_GetObjectItem(first_choice, "message");
                if (message != NULL) {
                    cJSON *content = cJSON_GetObjectItem(message, "content");
                    if (content != NULL && cJSON_IsString(content)) {
                        const char *content_str = cJSON_GetStringValue(content);

                        // Check if the content contains ```json and ```
                        if (strstr(content_str, "```json") != NULL) {
                            // Find the start of the actual JSON (after ```json)
                            const char *json_start = strstr(content_str, "```json") + 7;
                            // Find the end of the JSON (before the closing ```)
                            const char *json_end = strstr(json_start, "```");

                            if (json_start != NULL && json_end != NULL) {
                                // Create a temporary buffer for the JSON
                                size_t json_len = json_end - json_start;
                                char *json_buffer = (char *)malloc(json_len + 1);
                                if (json_buffer != NULL) {
                                    // Copy the JSON part to the buffer
                                    strncpy(json_buffer, json_start, json_len);
                                    json_buffer[json_len] = '\0';

                                    // Parse the JSON
                                    cJSON *detection_json = cJSON_Parse(json_buffer);
                                    if (detection_json != NULL) {
                                        cJSON *objects = cJSON_GetObjectItem(detection_json, "objects");
                                        if (objects != NULL && cJSON_IsArray(objects)) {
                                            int obj_count = cJSON_GetArraySize(objects);
                                            ESP_LOGI(TAG, "Found %d objects in the image", obj_count);

                                            // Process each detected object
                                            for (int i = 0; i < obj_count; i++) {
                                                cJSON *obj = cJSON_GetArrayItem(objects, i);
                                                if (obj != NULL) {
                                                    cJSON *category = cJSON_GetObjectItem(obj, "category");
                                                    cJSON *bbox = cJSON_GetObjectItem(obj, "bbox_2d");

                                                    if (category != NULL && cJSON_IsString(category) &&
                                                            bbox != NULL && cJSON_IsArray(bbox) && cJSON_GetArraySize(bbox) == 4) {

                                                        // Extract bounding box coordinates
                                                        int x1 = cJSON_GetArrayItem(bbox, 0)->valueint;
                                                        int y1 = cJSON_GetArrayItem(bbox, 1)->valueint;
                                                        int x2 = cJSON_GetArrayItem(bbox, 2)->valueint;
                                                        int y2 = cJSON_GetArrayItem(bbox, 3)->valueint;

                                                        // Create a BoundingBox object
                                                        BoundingBox box = {x1, y1, x2, y2};

                                                        // Store the object and its coordinates in the map
                                                        std::string category_str = cJSON_GetStringValue(category);
                                                        result.objects[category_str] = box;
                                                    }
                                                }
                                            }

                                            // Print all objects in the map
                                            ESP_LOGI(TAG, "All detected objects in map:");
                                            for (const auto &pair : result.objects) {
                                                ESP_LOGI(TAG, "  %s: [%d, %d, %d, %d]",
                                                         pair.first.c_str(),
                                                         pair.second.x1, pair.second.y1,
                                                         pair.second.x2, pair.second.y2);
                                            }
                                        }
                                        cJSON_Delete(detection_json);
                                    } else {
                                        ESP_LOGE(TAG, "Failed to parse detection JSON");
                                    }
                                    free(json_buffer);
                                }
                            } else {
                                ESP_LOGE(TAG, "Could not find JSON markers in content");
                            }
                        } else {
                            // Try to parse the content directly if it doesn't have the markers
                            cJSON *detection_json = cJSON_Parse(content_str);
                            if (detection_json != NULL) {
                                // Process the JSON as before
                                cJSON *objects = cJSON_GetObjectItem(detection_json, "objects");
                                if (objects != NULL && cJSON_IsArray(objects)) {
                                    int obj_count = cJSON_GetArraySize(objects);
                                    ESP_LOGI(TAG, "Found %d objects in the image", obj_count);

                                    // Process each detected object
                                    for (int i = 0; i < obj_count; i++) {
                                        cJSON *obj = cJSON_GetArrayItem(objects, i);
                                        if (obj != NULL) {
                                            cJSON *category = cJSON_GetObjectItem(obj, "category");
                                            cJSON *bbox = cJSON_GetObjectItem(obj, "bbox_2d");

                                            if (category != NULL && cJSON_IsString(category) &&
                                                    bbox != NULL && cJSON_IsArray(bbox) && cJSON_GetArraySize(bbox) == 4) {

                                                // Extract bounding box coordinates
                                                int x1 = cJSON_GetArrayItem(bbox, 0)->valueint;
                                                int y1 = cJSON_GetArrayItem(bbox, 1)->valueint;
                                                int x2 = cJSON_GetArrayItem(bbox, 2)->valueint;
                                                int y2 = cJSON_GetArrayItem(bbox, 3)->valueint;

                                                // Create a BoundingBox object
                                                BoundingBox box = {x1, y1, x2, y2};

                                                // Store the object and its coordinates in the map
                                                std::string category_str = cJSON_GetStringValue(category);
                                                result.objects[category_str] = box;
                                            }
                                        }
                                    }

                                    // Print all objects in the map
                                    ESP_LOGI(TAG, "All detected objects in map:");
                                    for (const auto &pair : result.objects) {
                                        ESP_LOGI(TAG, "  %s: [%d, %d, %d, %d]",
                                                 pair.first.c_str(),
                                                 pair.second.x1, pair.second.y1,
                                                 pair.second.x2, pair.second.y2);
                                    }
                                }
                                cJSON_Delete(detection_json);
                            } else {
                                ESP_LOGE(TAG, "Failed to parse content as JSON");
                            }
                        }
                    }
                }
            }
        }
        cJSON_Delete(root);
    } else {
        ESP_LOGE(TAG, "Failed to parse JSON response");
    }

    return result;
}
