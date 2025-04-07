/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ui.h"
#include "esp_log.h"
#include "app_cam.h"
#include "bsp/esp-bsp.h"
#include "app_manager.hpp"
#include "cJSON.h"

static const char* TAG = "app_manager";
TaskHandle_t Manager::m_task_handle = nullptr;
Manager* g_manager_instance = nullptr;

Manager::Manager()
{
    m_qwen_vl = std::make_unique<QwenVL>();
    m_response_queue = xQueueCreate(5, sizeof(qwen_response_t));
    m_qwen_vl->set_response_queue(m_response_queue);
    m_mutex = xSemaphoreCreateMutex();

    m_jpeg_dec_config.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
    m_jpeg_dec_config.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
    m_jpeg_dec_config.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;

    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .timeout_ms = 500,
    };
    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&decode_eng_cfg, &m_jpeg_dec_handle));
    jpeg_decode_memory_alloc_cfg_t jpeg_dec_output_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };

    m_jpeg_dec_output_buf = (uint8_t *)jpeg_alloc_decoder_mem(640 * 480 * 2, &jpeg_dec_output_mem_cfg, &m_jpeg_dec_output_buf_alloced_size);

    // Bind lvgl button event handler
    lv_obj_add_event_cb(ui_RegfButton, lvgl_btn_event_handler, LV_EVENT_CLICKED, (void *)manager_event_t::RECOGNIZE);
    lv_obj_add_event_cb(ui_ClearButton, lvgl_btn_event_handler, LV_EVENT_CLICKED, (void *)manager_event_t::CLEAR);

    // Create draw container
    m_draw_container = lv_obj_create(ui_Screen1);
    lv_obj_set_size(m_draw_container, 640, 480);
    lv_obj_set_pos(m_draw_container, 0, 0);
    lv_obj_set_style_bg_opa(m_draw_container, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(m_draw_container, 0, 0);        // No border
    lv_obj_set_style_pad_all(m_draw_container, 0, 0);            // No padding

    // Add click event handler to draw container
    lv_obj_add_event_cb(m_draw_container, draw_container_click_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(m_draw_container, LV_OBJ_FLAG_CLICKABLE);     // Make it clickable

    g_manager_instance = this;
}

Manager::~Manager()
{
    if (m_jpeg_dec_output_buf != NULL) {
        free(m_jpeg_dec_output_buf);
    }

    // Delete QwenVL instance
    m_qwen_vl.reset();

    // Delete response queue
    if (m_response_queue != NULL) {
        vQueueDelete(m_response_queue);
    }

    vSemaphoreDelete(m_mutex);
}

void Manager::draw_container_click_handler(lv_event_t *event)
{
    lv_point_t point;
    lv_indev_get_point(lv_indev_get_act(), &point);

    // Get the object that was clicked
    lv_obj_t *obj = lv_event_get_target(event);

    // Get the coordinates relative to the clicked object
    lv_coord_t x = point.x - lv_obj_get_x(obj);
    lv_coord_t y = point.y - lv_obj_get_y(obj);

    ESP_LOGI(TAG, "Draw container clicked at coordinates: x=%d, y=%d (relative to container)", x, y);
    ESP_LOGI(TAG, "Absolute screen coordinates: x=%d, y=%d", point.x, point.y);

    Manager *self = (Manager *)lv_event_get_user_data(event);
    self->m_click_coordinates_index = self->m_click_coordinates_index % 4;

    self->m_click_coordinates[self->m_click_coordinates_index].x = x;
    self->m_click_coordinates[self->m_click_coordinates_index].y = y;
    self->m_click_coordinates_index++;
}

void Manager::lcd_refresh_task(void *arg)
{
    Manager *self = (Manager *)arg;

    lv_img_dsc_t uvc_lv_img_dsc = {};
    uvc_lv_img_dsc.header.always_zero = 0;
    uvc_lv_img_dsc.header.w = 640;
    uvc_lv_img_dsc.header.h = 480;
    uvc_lv_img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    uvc_lv_img_dsc.data_size = 640 * 480 * 2;
    uvc_lv_img_dsc.data = NULL;

    // Create a style for bounding boxes
    static lv_style_t style_box;
    lv_style_init(&style_box);
    lv_style_set_bg_opa(&style_box, LV_OPA_TRANSP); // Transparent background
    lv_style_set_border_color(&style_box, lv_color_make(255, 0, 0)); // Red border
    lv_style_set_border_width(&style_box, 2); // 2 pixel border width
    lv_style_set_border_side(&style_box, LV_BORDER_SIDE_FULL); // All sides

    // Create a style for object names
    static lv_style_t style_label;
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, lv_color_make(255, 0, 0)); // Red color
    lv_style_set_bg_color(&style_label, lv_color_make(0, 0, 0)); // Black background
    lv_style_set_bg_opa(&style_label, LV_OPA_50); // Semi-transparent background
    lv_style_set_pad_all(&style_label, 2); // Padding

    // Create a style for click points (solid circles)
    static lv_style_t style_circle;
    lv_style_init(&style_circle);
    lv_style_set_bg_color(&style_circle, lv_color_make(255, 0, 0)); // Red color
    lv_style_set_bg_opa(&style_circle, LV_OPA_COVER); // Fully opaque
    lv_style_set_radius(&style_circle, LV_RADIUS_CIRCLE); // Make it a perfect circle

    // Create a style for coordinate labels
    static lv_style_t style_coord_label;
    lv_style_init(&style_coord_label);
    lv_style_set_text_color(&style_coord_label, lv_color_make(255, 0, 0)); // Red color
    lv_style_set_bg_color(&style_coord_label, lv_color_make(0, 0, 0)); // Black background
    lv_style_set_bg_opa(&style_coord_label, LV_OPA_50); // Semi-transparent background
    lv_style_set_pad_all(&style_coord_label, 2); // Padding
    lv_style_set_text_align(&style_coord_label, LV_TEXT_ALIGN_CENTER); // Center text

    while (1) {
        uvc_host_frame_t *frame = app_uvc_get_frame_by_index(0);
        if (frame != NULL) {
            esp_err_t res = jpeg_decoder_process(self->m_jpeg_dec_handle, &self->m_jpeg_dec_config, frame->data, frame->data_len, self->m_jpeg_dec_output_buf, self->m_jpeg_dec_output_buf_alloced_size, &self->m_jpeg_decoded_size);
            if (res != ESP_OK) {
                ESP_LOGE(TAG, "failed to decode jpeg image");
                app_uvc_return_frame_by_index(0, frame);
                continue;
            }

            bsp_display_lock(0);
            uvc_lv_img_dsc.data = self->m_jpeg_dec_output_buf;
            lv_img_set_src(ui_uvcframe, &uvc_lv_img_dsc);

            // Clear previous drawings
            lv_obj_clean(self->m_draw_container);

            // Draw bounding boxes for detected objects
            if (!self->m_detected_objects.empty()) {
                for (const auto &pair : self->m_detected_objects) {
                    const std::string &category = pair.first;
                    const BoundingBox &box = pair.second;

                    // Create a rectangle object for the bounding box
                    lv_obj_t *rect = lv_obj_create(self->m_draw_container);
                    lv_obj_set_size(rect, box.x2 - box.x1, box.y2 - box.y1);
                    lv_obj_set_pos(rect, box.x1, box.y1);
                    lv_obj_add_style(rect, &style_box, 0);

                    // Create a label for the object category
                    lv_obj_t *label = lv_label_create(self->m_draw_container);
                    lv_label_set_text(label, category.c_str());
                    lv_obj_add_style(label, &style_label, 0);
                    lv_obj_set_pos(label, box.x1, box.y1 - 20); // Position above the bounding box
                }
            }

            // Draw click coordinates as solid circles with labels
            for (const auto &pair : self->m_click_coordinates) {
                int index = pair.first;
                int x = pair.second.x;
                int y = pair.second.y;

                // Create a circle for the click point
                lv_obj_t *circle = lv_obj_create(self->m_draw_container);
                lv_obj_set_size(circle, 10, 10);
                lv_obj_set_pos(circle, x - 5, y - 5); // Center the circle on the click point
                lv_obj_add_style(circle, &style_circle, 0);

                // Create a label for the coordinates and index
                lv_obj_t *label = lv_label_create(self->m_draw_container);
                char coord_text[32];
                snprintf(coord_text, sizeof(coord_text), "#%d (%d,%d)", index, x, y);
                lv_label_set_text(label, coord_text);
                lv_obj_add_style(label, &style_coord_label, 0);
                lv_obj_set_pos(label, x - 30, y - 25); // Position above the circle
            }

            bsp_display_unlock();

            app_uvc_return_frame_by_index(0, frame);
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

void Manager::event_handle_task(void *arg)
{
    Manager *self = (Manager *)arg;

    while (1) {
        // Check for messages in the response queue first
        qwen_response_t response;
        if (xQueueReceive(self->m_response_queue, &response, 0) == pdTRUE) {
            ESP_LOGI(TAG, "Received response from QwenVL: %s", response.data);

            self->m_detected_objects.clear();

            // Use QwenVL's parse method to get object detection results
            ObjectDetectionResult result = QwenVL::parse_object_detection_json(response.data);

            // Update the detected objects map with the parsed results
            self->m_detected_objects = result.objects;

            free(response.data);
        }

        // Then handle task notifications
        uint32_t event;
        if (xTaskNotifyWait(0, 0xffffffff, &event, 1) == pdTRUE) {
            if (event & static_cast<uint32_t>(manager_event_t::RECOGNIZE)) {
                uvc_host_frame_t *frame = app_uvc_get_frame_by_index(0);
                if (frame != NULL) {
                    self->m_qwen_vl->run((const char*)(frame->data), frame->data_len);
                    app_uvc_return_frame_by_index(0, frame);
                }
            } else if (event & static_cast<uint32_t>(manager_event_t::CLEAR)) {
                // Clear the detected objects map
                self->m_detected_objects.clear();
                self->m_click_coordinates.clear();
                self->m_click_coordinates_index = 0;
                ESP_LOGI(TAG, "Cleared all detected objects");
            }
        }
    }
}

void Manager::lvgl_btn_event_handler(lv_event_t *event)
{
    manager_event_t manager_event = (manager_event_t)(reinterpret_cast<int>(lv_event_get_user_data(event)));
    switch (manager_event) {
    case manager_event_t::RECOGNIZE:
        xTaskNotify(m_task_handle, (uint32_t)manager_event_t::RECOGNIZE, eSetBits);
        break;
    case manager_event_t::CLEAR:
        xTaskNotify(m_task_handle, (uint32_t)manager_event_t::CLEAR, eSetBits);
        break;
    default:
        break;
    }
}

void Manager::run()
{
    if (xTaskCreatePinnedToCore(lcd_refresh_task, "lcd_refresh_task", 10 * 1024, this, 5, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "failed to create lcd_refresh_task");
        return;
    }

    if (xTaskCreatePinnedToCore(event_handle_task, "event_handle_task", 10 * 1024, this, 5, &m_task_handle, 0) != pdPASS) {
        ESP_LOGE(TAG, "failed to create event_handle_task");
        return;
    }
}
