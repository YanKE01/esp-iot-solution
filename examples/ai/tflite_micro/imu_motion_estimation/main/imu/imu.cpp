/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "imu.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "IMU";

float imu_lsb_to_dps(int16_t val, float full_scale_dps, uint8_t bit_width)
{
    const float half_scale = (float)((1u << bit_width) / 2u);
    return (full_scale_dps / half_scale) * (float)val;
}

int8_t imu_bmi270_gyro_config(bmi270_handle_t bmi_handle)
{
    int8_t rslt = BMI2_OK;
    if (bmi_handle == nullptr) {
        return BMI2_E_NULL_PTR;
    }

    struct bmi2_sens_config config[2];
    config[0].type = BMI2_ACCEL;
    config[1].type = BMI2_GYRO;

    rslt = bmi2_get_sensor_config(config, 2, (struct bmi2_dev *)bmi_handle);
    ESP_RETURN_ON_FALSE(rslt == BMI2_OK, rslt, TAG, "bmi2_get_sensor_config failed");

    rslt = bmi2_map_data_int(BMI2_DRDY_INT, BMI2_INT1, (struct bmi2_dev *)bmi_handle);
    ESP_RETURN_ON_FALSE(rslt == BMI2_OK, rslt, TAG, "bmi2_map_data_int failed");

    if (rslt == BMI2_OK) {
        // Acc
        config[0].cfg.acc.odr         = BMI2_ACC_ODR_200HZ;
        config[0].cfg.acc.range       = BMI2_ACC_RANGE_2G;
        config[0].cfg.acc.bwp         = BMI2_ACC_NORMAL_AVG4;
        config[0].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;

        // Gyro
        config[1].cfg.gyr.odr         = BMI2_GYR_ODR_200HZ;
        config[1].cfg.gyr.range       = BMI2_GYR_RANGE_2000;
        config[1].cfg.gyr.bwp         = BMI2_GYR_NORMAL_MODE;
        config[1].cfg.gyr.noise_perf  = BMI2_POWER_OPT_MODE;
        config[1].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;

        rslt = bmi2_set_sensor_config(config, 2, (struct bmi2_dev *)bmi_handle);
        bmi2_error_codes_print_result(rslt);
    }

    return rslt;
}

bmi270_handle_t imu_bmi270_init(i2c_bus_handle_t i2c_bus)
{
    ESP_RETURN_ON_FALSE(i2c_bus, nullptr, TAG, "Invalid argument");

    bmi270_handle_t bmi_handle = (bmi270_handle_t)calloc(1, sizeof(bmi2_dev));
    ESP_RETURN_ON_FALSE(bmi_handle, nullptr, TAG, "Failed to allocate memory for bmi270 handle");

    bmi270_i2c_config_t i2c_bmi270_conf = {};
    i2c_bmi270_conf.i2c_handle = i2c_bus;
    i2c_bmi270_conf.i2c_addr = BMI270_I2C_ADDRESS;

    if (bmi270_sensor_create(&i2c_bmi270_conf, &bmi_handle) != ESP_OK) {
        ESP_LOGE(TAG, "bmi270_sensor_create failed");
        free(bmi_handle);
        return nullptr;
    }

    if (imu_bmi270_gyro_config(bmi_handle) != BMI2_OK) {
        ESP_LOGE(TAG, "imu_bmi270_gyro_config failed");
        free(bmi_handle);
        return nullptr;
    }

    uint8_t sensor_list[2] = { BMI2_ACCEL, BMI2_GYRO };
    int8_t rslt = bmi2_sensor_enable(sensor_list, 2, bmi_handle);
    bmi2_error_codes_print_result(rslt);
    if (rslt != BMI2_OK) {
        ESP_LOGE(TAG, "bmi2_sensor_enable failed");
        free(bmi_handle);
        return nullptr;
    }

    ESP_LOGI(TAG, "Imu bmi270 init success");
    return bmi_handle;
}
