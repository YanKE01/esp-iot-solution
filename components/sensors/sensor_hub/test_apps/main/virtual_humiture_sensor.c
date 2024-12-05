/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "iot_sensor_hub.h"
#include "esp_err.h"
#include "esp_random.h"

esp_err_t virtual_humiture_init(i2c_bus_handle_t i2c_bus)
{
    return ESP_OK;
}

esp_err_t virtual_humiture_deinit(void)
{
    return ESP_OK;
}

esp_err_t virtual_humiture_test(void)
{
    return ESP_OK;
}

esp_err_t virtual_humiture_acquire_humidity(float *h)
{
    *h = ((float)esp_random() / (float)UINT32_MAX) * 100.0f;
    return ESP_OK;
}

esp_err_t virtual_humiture_acquire_humidity1(float *h)
{
    *h = 22.0f;
    return ESP_OK;
}

esp_err_t virtual_humiture_acquire_temperature(float *t)
{
    *t = ((float)esp_random() / (float)UINT32_MAX) * 100.0f;
    return ESP_OK;
}

esp_err_t virtual_humiture_acquire_temperature1(float *t)
{
    *t = 33.0f;
    return ESP_OK;
}

esp_err_t virtual_humiture_null_function(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static humiture_impl_t virtual_sht3x_impl = {
    .name = "virtual_sht3x",
    .sensor_type = HUMITURE_ID,
    .init = virtual_humiture_init,
    .deinit = virtual_humiture_deinit,
    .test = virtual_humiture_test,
    .acquire_humidity = virtual_humiture_acquire_humidity,
    .acquire_temperature = virtual_humiture_acquire_temperature,
    .sleep = virtual_humiture_null_function,
    .wakeup = virtual_humiture_null_function,
};

static humiture_impl_t virtual_hts221_impl = {
    .name = "virtual_hts221",
    .sensor_type = HUMITURE_ID,
    .init = virtual_humiture_init,
    .deinit = virtual_humiture_deinit,
    .test = virtual_humiture_test,
    .acquire_humidity = virtual_humiture_acquire_humidity1,
    .acquire_temperature = virtual_humiture_acquire_temperature1,
    .sleep = virtual_humiture_null_function,
    .wakeup = virtual_humiture_null_function,
};

void *virtual_sht3x_detect(sensor_info_t *sensor_info)
{
    sensor_info->snesor_type = HUMITURE_ID;
    sensor_info->name = virtual_sht3x_impl.name;
    return (void*)&virtual_sht3x_impl;
}

ESP_SENSOR_DETECT_FN(virtual_sht3x_detect)
{
    return virtual_sht3x_detect(sensor_info);
}

void *virtual_hst221_detect(sensor_info_t *sensor_info)
{
    sensor_info->snesor_type = HUMITURE_ID;
    sensor_info->name = virtual_hts221_impl.name;
    return (void*)&virtual_hts221_impl;
}

ESP_SENSOR_DETECT_FN(virtual_hst221_detect)
{
    return virtual_hst221_detect(sensor_info);
}
