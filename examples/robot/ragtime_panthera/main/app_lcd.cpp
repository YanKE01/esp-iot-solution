/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_lcd.h"
#include "esp_log.h"
#include "private/esp_brookesia_utils.h"

static const char *TAG = "app_lcd";

ESP_Brookesia_Phone* app_lcd_init(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .hw_cfg = {
#if CONFIG_BSP_LCD_TYPE_HDMI
#if CONFIG_BSP_LCD_HDMI_800x600_60HZ
            .hdmi_resolution = BSP_HDMI_RES_800x600,
#elif CONFIG_BSP_LCD_HDMI_1280x720_60HZ
            .hdmi_resolution = BSP_HDMI_RES_1280x720,
#elif CONFIG_BSP_LCD_HDMI_1280x800_60HZ
            .hdmi_resolution = BSP_HDMI_RES_1280x800,
#elif CONFIG_BSP_LCD_HDMI_1920x1080_30HZ
            .hdmi_resolution = BSP_HDMI_RES_1920x1080,
#endif
#else
            .hdmi_resolution = BSP_HDMI_RES_NONE,
#endif
            .dsi_bus = {
                .lane_bit_rate_mbps = BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS,
            }
        },
        .flags = {
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
            .buff_dma = false,
#else
            .buff_dma = true,
#endif
            .buff_spiram = false,
            .sw_rotate = true,
        }
    };

    cfg.lvgl_port_cfg.task_priority = 5;
    cfg.lvgl_port_cfg.task_affinity = 1;

    bsp_display_start_with_config(&cfg);
    bsp_display_brightness_set(100);
    bsp_display_backlight_on();

    bsp_display_lock(0);
    ESP_Brookesia_Phone *phone = new ESP_Brookesia_Phone();
    ESP_BROOKESIA_CHECK_NULL_RETURN(phone, nullptr, "Create phone failed");

    ESP_Brookesia_PhoneStylesheet_t *stylesheet = new ESP_Brookesia_PhoneStylesheet_t(ESP_BROOKESIA_PHONE_1024_600_DARK_STYLESHEET());
    ESP_BROOKESIA_CHECK_NULL_RETURN(stylesheet, nullptr, "Create stylesheet failed");

    if (stylesheet != nullptr) {
        ESP_LOGI(TAG, "Using stylesheet (%s)", stylesheet->core.name);
        ESP_BROOKESIA_CHECK_FALSE_RETURN(phone->addStylesheet(stylesheet), nullptr, "Add stylesheet failed");
        ESP_BROOKESIA_CHECK_FALSE_RETURN(phone->activateStylesheet(stylesheet), nullptr, "Activate stylesheet failed");
        delete stylesheet;
    }

    ESP_BROOKESIA_CHECK_FALSE_RETURN(phone->setTouchDevice(bsp_display_get_input_dev()), nullptr, "Set touch device failed");
    phone->registerLvLockCallback((ESP_Brookesia_GUI_LockCallback_t)(bsp_display_lock), 0);
    phone->registerLvUnlockCallback((ESP_Brookesia_GUI_UnlockCallback_t)(bsp_display_unlock));
    ESP_BROOKESIA_CHECK_FALSE_RETURN(phone->begin(), nullptr, "Begin failed");

    bsp_display_unlock();

    return phone;
}
