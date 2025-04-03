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

static const char* TAG = "app_manager";
TaskHandle_t Manager::m_task_handle = nullptr;
Manager* g_manager_instance = nullptr;

Manager::Manager()
{
    m_qwen_vl = std::make_unique<QwenVL>();
    m_mutex = xSemaphoreCreateMutex();

    m_jpeg_dec_config.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
    m_jpeg_dec_config.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
    m_jpeg_dec_config.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;

    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .timeout_ms = 40,
    };
    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&decode_eng_cfg, &m_jpeg_dec_handle));
    jpeg_decode_memory_alloc_cfg_t jpeg_dec_output_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };

    m_jpeg_dec_output_buf = (uint8_t *)jpeg_alloc_decoder_mem(640 * 480 * 2, &jpeg_dec_output_mem_cfg, &m_jpeg_dec_output_buf_alloced_size);

    // Bind lvgl button event handler
    lv_obj_add_event_cb(ui_RegfButton, lvgl_btn_event_handler, LV_EVENT_CLICKED, (void *)manager_event_t::RECOGNIZE);
    lv_obj_add_event_cb(ui_ClearButton, lvgl_btn_event_handler, LV_EVENT_CLICKED, (void *)manager_event_t::CLEAR);

    g_manager_instance = this;
}

Manager::~Manager()
{
    if (m_jpeg_dec_output_buf != NULL) {
        free(m_jpeg_dec_output_buf);
    }

    vSemaphoreDelete(m_mutex);
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
            bsp_display_unlock();

            app_uvc_return_frame_by_index(0, frame);
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

void Manager::event_handle_task(void *arg)
{
    Manager *self = (Manager *)arg;

    while (1) {
        uint32_t event;
        xTaskNotifyWait(0, 0xffffffff, &event, portMAX_DELAY);
        if (event & static_cast<uint32_t>(manager_event_t::RECOGNIZE)) {
            uvc_host_frame_t *frame = app_uvc_get_frame_by_index(0);
            if (frame != NULL) {
                self->m_qwen_vl->run((const char*)(frame->data), frame->data_len);
                app_uvc_return_frame_by_index(0, frame);
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
