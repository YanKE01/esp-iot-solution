/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "panthera_grasp.h"
#include "ui/ui.h"

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
}

PantheraGrasp::~PantheraGrasp()
{
}

bool PantheraGrasp::init(void)
{
    return true;
}

bool PantheraGrasp::run(void)
{
    ui_panthera_grasp_init();
    return true;
}

bool PantheraGrasp::pause(void)
{
    return true;
}

bool PantheraGrasp::resume(void)
{
    return true;
}

bool PantheraGrasp::back(void)
{
    notifyCoreClosed();
    return true;
}

bool PantheraGrasp::close(void)
{
    return true;
}
