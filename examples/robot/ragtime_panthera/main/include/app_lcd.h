/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_brookesia.hpp"

/**
 * @brief Initialize the LCD panel.
 *
 * @return ESP_Brookesia_Phone* Return the phone object if initialized successfully, return NULL if failed.
 */
ESP_Brookesia_Phone* app_lcd_init(void);
