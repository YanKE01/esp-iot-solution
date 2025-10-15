/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "i2c_bus.h"
#include "bmi270.h"
#include "common/common.h"

float imu_lsb_to_dps(int16_t val, float full_scale_dps, uint8_t bit_width);
bmi270_handle_t imu_bmi270_init(i2c_bus_handle_t i2c_bus);
