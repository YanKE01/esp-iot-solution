/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "bmi270.h"

void model_imu_init();
void model_imu_predict_start(bmi270_handle_t bmi270);
