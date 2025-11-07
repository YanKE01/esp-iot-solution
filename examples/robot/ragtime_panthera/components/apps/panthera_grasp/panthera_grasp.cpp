/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "ui/ui.h"
#include "ui/screens/ui_RobotGraspScreen.h"
#include "usb_camera.h"
#include "panthera_grasp.h"

LV_IMG_DECLARE(img_panthera_grasp);

PantheraGrasp::PantheraGrasp(damiao::Motor_Control* motor_control)
    : ESP_Brookesia_PhoneApp(
          []()
{
    ESP_Brookesia_CoreAppData_t core_data = {
        .name = "Panthera Grasp",
        .launcher_icon = ESP_BROOKESIA_STYLE_IMAGE(&img_panthera_grasp),
        .screen_size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100),
        .flags = {
            .enable_default_screen = 0,
            .enable_recycle_resource = 1,
            .enable_resize_visual_area = 1,
        },
    };
    return core_data;
}(),
[]()
{
    ESP_Brookesia_PhoneAppData_t phone_data = {
        .app_launcher_page_index = 0,
        .status_icon_area_index = 0,
        .status_icon_data = {
            .size = {},
            .icon = {
                .image_num = 1,
                .images = {
                    ESP_BROOKESIA_STYLE_IMAGE(&img_panthera_grasp),
                },
            },
        },
        .status_bar_visual_mode = ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_HIDE,
        .navigation_bar_visual_mode = ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_HIDE,
        .flags = {
            .enable_status_icon_common_size = 1,
            .enable_navigation_gesture = 1,
        },
    };
    return phone_data;
}()
  ),
motor_control_(motor_control)
{
    lcd_refresh_task_handle_ = nullptr;
    detect_task_handle_ = nullptr;
    detect_queue_handle_ = nullptr;
    detect_results_mutex_ = nullptr;
    color_detect_ = nullptr;
}

PantheraGrasp::~PantheraGrasp()
{
}

bool PantheraGrasp::init(void)
{
    // Initialize the JPEG decoder
    jpeg_decode_cfg_.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
    jpeg_decode_cfg_.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
    jpeg_decode_cfg_.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .intr_priority = 0,
        .timeout_ms = 5000,
    };
    esp_err_t ret = jpeg_new_decoder_engine(&decode_eng_cfg, &jpeg_decoder_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE("panthera_grasp", "JPEG decoder init failed: %s", esp_err_to_name(ret));
        return false;
    }

    jpeg_decode_memory_alloc_cfg_t jpeg_dec_output_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };

    jpeg_decode_buffer_ = (uint8_t*)jpeg_alloc_decoder_mem(640 * 480 * 2, &jpeg_dec_output_mem_cfg, &jpeg_decode_buffer_size_);
    if (jpeg_decode_buffer_ == NULL) {
        ESP_LOGE("panthera_grasp", "JPEG decoder buffer allocation failed");
        return false;
    }

    // Initialize the USB camera
    ret = usb_camera_init(640, 480);
    if (ret != ESP_OK) {
        ESP_LOGE("panthera_grasp", "USB camera init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Initialize the color detect
    if (color_detect_ == nullptr) {
        color_detect_ = new ColorDetect(640, 480);
        if (color_detect_ == nullptr) {
            ESP_LOGE("panthera_grasp", "Color detect init failed");
            return false;
        }
        color_detect_->register_color({170, 100, 100}, {10, 255, 255}, "red");
    }

    // Create detect queue if it doesn't exist
    if (detect_queue_handle_ == nullptr) {
        detect_queue_handle_ = xQueueCreate(20, sizeof(detect_queue_msg_t));
        if (detect_queue_handle_ == nullptr) {
            ESP_LOGE("panthera_grasp", "Failed to create detect queue");
            return false;
        }
        ESP_LOGI("panthera_grasp", "Detect queue created successfully");
    }

    // Create mutex for detection results if it doesn't exist
    if (detect_results_mutex_ == nullptr) {
        detect_results_mutex_ = xSemaphoreCreateMutex();
        if (detect_results_mutex_ == nullptr) {
            ESP_LOGE("panthera_grasp", "Failed to create detect results mutex");
            return false;
        }
        ESP_LOGI("panthera_grasp", "Detect results mutex created successfully");
    }

    ESP_LOGI("panthera_grasp", "Init");
    return true;
}

bool PantheraGrasp::run(void)
{
    ESP_LOGI("panthera_grasp", "Run");
    ui_panthera_grasp_init();

    if (lcd_refresh_task_handle_ == nullptr) {
        BaseType_t ret = xTaskCreatePinnedToCore(lcd_refresh_task, "lcd_refresh_task", 10 * 1024, this, 6, &lcd_refresh_task_handle_, 0);
        if (ret != pdPASS) {
            ESP_LOGE("panthera_grasp", "Failed to create LCD refresh task");
            return false;
        }
        ESP_LOGI("panthera_grasp", "LCD refresh task created successfully");
    }

    if (detect_task_handle_ == nullptr) {
        BaseType_t ret = xTaskCreatePinnedToCore(detect_task, "detect_task", 10 * 1024, this, 5, &detect_task_handle_, 0);
        if (ret != pdPASS) {
            ESP_LOGE("panthera_grasp", "Failed to create detect task");
            return false;
        }
        ESP_LOGI("panthera_grasp", "Detect task created successfully");
    }

    return true;
}

bool PantheraGrasp::pause(void)
{
    ESP_LOGI("panthera_grasp", "Pause");
    return true;
}

bool PantheraGrasp::resume(void)
{
    ESP_LOGI("panthera_grasp", "Resume");
    return true;
}

bool PantheraGrasp::back(void)
{
    ESP_LOGI("panthera_grasp", "Back");
    notifyCoreClosed();
    return true;
}

bool PantheraGrasp::close(void)
{
    ESP_LOGI("panthera_grasp", "Close");

    return true;
}

void PantheraGrasp::lcd_refresh_task(void* pvParameters)
{
    PantheraGrasp* self = static_cast<PantheraGrasp*>(pvParameters);
    if (self == nullptr) {
        ESP_LOGE("panthera_grasp", "Invalid app instance in LCD refresh task");
        vTaskDelete(NULL);
        return;
    }

    // create lvgl image descriptor
    lv_image_dsc_t uvc_lv_img_dsc = {};
    uvc_lv_img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    uvc_lv_img_dsc.header.w = 640;
    uvc_lv_img_dsc.header.h = 480;
    uvc_lv_img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    uvc_lv_img_dsc.header.flags = 0;
    uvc_lv_img_dsc.data_size = 640 * 480 * 2;
    uvc_lv_img_dsc.data = NULL;

    // Create overlay container for drawing detection boxes
    bsp_display_lock(0);
    lv_obj_t* overlay_container = lv_obj_create(ui_Container2);
    lv_obj_remove_style_all(overlay_container);
    lv_obj_set_size(overlay_container, 640, 480);
    lv_obj_set_pos(overlay_container, 0, 0);
    lv_obj_set_align(overlay_container, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(overlay_container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(overlay_container, LV_OPA_TRANSP, 0);
    lv_obj_move_to_index(overlay_container, -1);  // Move to top (above ui_uvcframe)
    bsp_display_unlock();

    // Store rectangle objects for reuse (max 10 detection boxes)
    const int max_boxes = 10;
    lv_obj_t* box_objects[max_boxes] = {nullptr};
    int box_count = 0;

    ESP_LOGI("panthera_grasp", "LCD refresh task started");

    while (1) {
        uvc_host_frame_t *frame = usb_camera_get_frame();
        if (frame == NULL) {
            // Add delay to avoid blocking CPU and allow other tasks to run
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        // Validate frame data before decoding
        if (frame->data == NULL) {
            ESP_LOGW("panthera_grasp", "Frame data is NULL, skipping");
            usb_camera_return_frame(frame);
            continue;
        }

        // Check JPEG magic bytes (SOI marker: 0xFF 0xD8)
        if (frame->data_len < 2 || frame->data[0] != 0xFF || frame->data[1] != 0xD8) {
            ESP_LOGW("panthera_grasp", "Invalid JPEG header, skipping frame");
            usb_camera_return_frame(frame);
            continue;
        }

        // Decode the JPEG
        esp_err_t ret = jpeg_decoder_process(self->jpeg_decoder_handle_, &self->jpeg_decode_cfg_, frame->data, frame->data_len, self->jpeg_decode_buffer_, self->jpeg_decode_buffer_size_, &self->jpeg_decoded_size_);
        if (ret != ESP_OK) {
            ESP_LOGW("panthera_grasp", "JPEG decoder process failed: %s, skipping frame", esp_err_to_name(ret));
            usb_camera_return_frame(frame);
            continue;
        }

        bsp_display_lock(0);
        uvc_lv_img_dsc.data = self->jpeg_decode_buffer_;
        lv_image_set_src(ui_uvcframe, &uvc_lv_img_dsc);

        // Read detection results and draw boxes
        std::list<dl::detect::result_t> current_results;
        if (self->detect_results_mutex_ != nullptr) {
            if (xSemaphoreTake(self->detect_results_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
                current_results.assign(self->detect_results_.begin(), self->detect_results_.end());
                xSemaphoreGive(self->detect_results_mutex_);
            }
        }

        // Clear previous boxes
        for (int i = 0; i < box_count; i++) {
            if (box_objects[i] != nullptr) {
                lv_obj_del(box_objects[i]);
                box_objects[i] = nullptr;
            }
        }
        box_count = 0;

        // Draw new detection boxes
        const int border_width = 3;
        lv_color_t box_color = lv_color_hex(0xFF0000);  // Red color

        for (const auto &result : current_results) {
            if (box_count >= max_boxes) {
                break;
            }

            int x1 = result.box[0];
            int y1 = result.box[1];
            int x2 = result.box[2];
            int y2 = result.box[3];
            int width = x2 - x1;
            int height = y2 - y1;

            // Create rectangle object with border
            lv_obj_t* box = lv_obj_create(overlay_container);
            lv_obj_remove_style_all(box);
            lv_obj_set_size(box, width, height);
            lv_obj_set_pos(box, x1, y1);
            lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);  // Transparent background
            lv_obj_set_style_border_color(box, box_color, 0);
            lv_obj_set_style_border_width(box, border_width, 0);
            lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            box_objects[box_count] = box;

            box_count++;
        }

        bsp_display_unlock();

        // Send decoded image to detect queue (non-blocking)
        if (self->detect_queue_handle_ != nullptr) {
            detect_queue_msg_t detect_msg;
            detect_msg.image_data = self->jpeg_decode_buffer_;
            detect_msg.image_size = self->jpeg_decoded_size_;
            xQueueSend(self->detect_queue_handle_, &detect_msg, pdMS_TO_TICKS(10));
        }

        usb_camera_return_frame(frame);
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

void PantheraGrasp::detect_task(void* pvParameters)
{
    PantheraGrasp* self = static_cast<PantheraGrasp*>(pvParameters);
    if (self == nullptr) {
        ESP_LOGE("panthera_grasp", "Invalid app instance in detect task");
        vTaskDelete(NULL);
        return;
    }

    // create dl image
    dl::image::img_t img = {
        .data = NULL,
        .width = 640,
        .height = 480,
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
    };

    ESP_LOGI("panthera_grasp", "Detect task started");

    detect_queue_msg_t msg;

    while (1) {
        // Wait for image data from queue
        if (xQueueReceive(self->detect_queue_handle_, &msg, portMAX_DELAY) == pdTRUE) {
            // Set image data from queue message
            img.data = msg.image_data;

            // Run color detection
            if (self->color_detect_ != nullptr) {
                auto &results = self->color_detect_->run(img);

                // Save results with mutex protection
                if (xSemaphoreTake(self->detect_results_mutex_, portMAX_DELAY) == pdTRUE) {
                    self->detect_results_.clear();
                    self->detect_results_.assign(results.begin(), results.end());
                    xSemaphoreGive(self->detect_results_mutex_);
                }
            }
        }
        vTaskDelay(30 / portTICK_PERIOD_MS);
    }
}
