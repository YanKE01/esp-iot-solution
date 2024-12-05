/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "iot_sensor_hub.h"
#include "esp_log.h"

static sensor_handle_t humiture_handle = NULL;
static sensor_event_handler_instance_t humiture_handler_handle = NULL;

static sensor_handle_t hts221_handle = NULL;
static sensor_event_handler_instance_t hts221_handler_handle = NULL;

static const char* TAG = "sensor_hub_test";

static void sensor_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    sensor_data_t *sensor_data = (sensor_data_t *)event_data;
    sensor_type_t sensor_type = (sensor_type_t)(sensor_data->sensor_type);

    if (sensor_type >= SENSOR_TYPE_MAX) {
        ESP_LOGE(TAG, "sensor %s invalid", sensor_data->sensor_name);
        return;
    }
    switch (id) {
    case SENSOR_STARTED:
        ESP_LOGI(TAG, "Timestamp = %llu - %s SENSOR_STARTED",
                 sensor_data->timestamp,
                 sensor_data->sensor_name);
        break;
    case SENSOR_STOPED:
        ESP_LOGI(TAG, "Timestamp = %llu - %s SENSOR_STOPED",
                 sensor_data->timestamp,
                 sensor_data->sensor_name);
        break;
    case SENSOR_HUMI_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_HUMI_DATA_READY - "
                 "humiture=%.2f",
                 sensor_data->timestamp,
                 sensor_data->humidity);
        break;
    case SENSOR_TEMP_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_TEMP_DATA_READY - "
                 "temperature=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->temperature);
        break;
    case SENSOR_ACCE_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_ACCE_DATA_READY - "
                 "acce_x=%.2f, acce_y=%.2f, acce_z=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->acce.x, sensor_data->acce.y, sensor_data->acce.z);
        break;
    case SENSOR_GYRO_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_GYRO_DATA_READY - "
                 "gyro_x=%.2f, gyro_y=%.2f, gyro_z=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->gyro.x, sensor_data->gyro.y, sensor_data->gyro.z);
        break;
    case SENSOR_LIGHT_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_LIGHT_DATA_READY - "
                 "light=%.2f",
                 sensor_data->timestamp,
                 sensor_data->light);
        break;
    case SENSOR_RGBW_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_RGBW_DATA_READY - "
                 "r=%.2f, g=%.2f, b=%.2f, w=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->rgbw.r, sensor_data->rgbw.r, sensor_data->rgbw.b, sensor_data->rgbw.w);
        break;
    case SENSOR_UV_DATA_READY:
        ESP_LOGI(TAG, "Timestamp = %llu - SENSOR_UV_DATA_READY - "
                 "uv=%.2f, uva=%.2f, uvb=%.2f\n",
                 sensor_data->timestamp,
                 sensor_data->uv.uv, sensor_data->uv.uva, sensor_data->uv.uvb);
        break;
    default:
        ESP_LOGI(TAG, "Timestamp = %llu - event id = %ld", sensor_data->timestamp, id);
        break;
    }
}

void app_main(void)
{
    sensor_config_t sensor_config = {
        .mode = MODE_POLLING,
        .min_delay = 200,
    };

    sensor_config_t sensor_config2 = {
        .mode = MODE_POLLING,
        .min_delay = 400,
    };
    TEST_ASSERT(ESP_OK == iot_sensor_create("virtual_sht3x", &sensor_config, &humiture_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_create("virtual_hts221", &sensor_config2, &hts221_handle));

    TEST_ASSERT(NULL != humiture_handle);
    TEST_ASSERT(NULL != hts221_handle);

    TEST_ASSERT(ESP_OK == iot_sensor_handler_register(humiture_handle, sensor_event_handler, &humiture_handler_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_handler_register(hts221_handle, sensor_event_handler, &hts221_handler_handle));

    TEST_ASSERT(ESP_OK == iot_sensor_start(humiture_handle));
    TEST_ASSERT(ESP_OK == iot_sensor_start(hts221_handle));

}
