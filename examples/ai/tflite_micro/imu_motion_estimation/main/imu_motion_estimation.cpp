/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "imu.h"
#include "model/predict.h"

extern "C" void app_main(void)
{
    // Initialize I2C bus
    i2c_config_t i2c_bus_conf = {};
    i2c_bus_conf.mode = I2C_MODE_MASTER;
    i2c_bus_conf.sda_io_num = GPIO_NUM_25;
    i2c_bus_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_bus_conf.scl_io_num = GPIO_NUM_26;
    i2c_bus_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_bus_conf.master.clk_speed = 200 * 1000;
    i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_bus_conf);

    // Initialize BMI270
    bmi270_handle_t bmi270 = imu_bmi270_init(i2c_bus);

    // Initialize model
    model_imu_init();

    // Start predict
    model_imu_predict_start(bmi270);
}
