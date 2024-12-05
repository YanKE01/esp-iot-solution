/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LIGHT_SENSOR_HAL_H_
#define _LIGHT_SENSOR_HAL_H_

#include "i2c_bus.h"
#include "esp_err.h"
#include "sensor_type.h"

typedef void *sensor_light_handle_t; /*!< light sensor handle*/

typedef struct {
    const char* name;
    esp_err_t (*init)(bus_handle_t);
    esp_err_t (*deinit)(void);
    esp_err_t (*test)(void);
    esp_err_t (*acquire_light)(float* l);
    esp_err_t (*acquire_rgbw)(float* r, float* g, float* b, float* w);
    esp_err_t (*acquire_uv)(float* uv, float* uva, float* uvb);
    esp_err_t (*sleep)(void);
    esp_err_t (*wakeup)(void);
} light_sensor_impl_t;

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Create a light sensor instance.
 * same series' sensor or sensor with same address can only be created once.
 *
 * @param bus i2c bus handle the sensor attached to
 * @param id id declared in light_sensor_id_t
 * @return sensor_light_handle_t return light sensor handle if succeed, return NULL if failed.
 */
sensor_light_handle_t light_sensor_create(bus_handle_t bus, sensor_device_impl_t device_impl);

/**
 * @brief Delete and release the sensor resource.
 *
 * @param sensor point to light sensor handle, will set to NULL if delete succeed.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t light_sensor_delete(sensor_light_handle_t *sensor);

/**
 * @brief Test if sensor is active.
 *
 * @param sensor light sensor handle to operate.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
*/
esp_err_t light_sensor_test(sensor_light_handle_t sensor);

/**
 * @brief Acquire light sensor illuminance result one time.
 *
 * @param sensor light sensor handle to operate.
 * @param lux result data (unit:lux)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t light_sensor_acquire_light(sensor_light_handle_t sensor, float *lux);

/**
 * @brief Acquire light sensor color result one time.
 * light color includes red green blue and white.
 *
 * @param sensor light sensor handle to operate.
 * @param rgbw result data (unit:lux)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t light_sensor_acquire_rgbw(sensor_light_handle_t sensor, rgbw_t *rgbw);

/**
 * @brief Acquire light sensor ultra violet result one time.
 * light Ultraviolet includes UVA UVB and UV.
 *
 * @param sensor light sensor handle to operate.
 * @param uv result data (unit:lux)
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t light_sensor_acquire_uv(sensor_light_handle_t sensor, uv_t *uv);

/**
 * @brief Set sensor to sleep mode.
 *
 * @param sensor light sensor handle to operate.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t light_sensor_sleep(sensor_light_handle_t sensor);

/**
 * @brief Wakeup sensor from sleep mode.
 *
 * @param sensor light sensor handle to operate.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
*/
esp_err_t light_sensor_wakeup(sensor_light_handle_t sensor);

/**
 * @brief acquire a group of sensor data
 *
 * @param sensor light sensor handle to operate
 * @param data_group acquired data
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t light_sensor_acquire(sensor_light_handle_t sensor, sensor_data_group_t *data_group);

/**
 * @brief control sensor mode with control commands and args
 *
 * @param sensor light sensor handle to operate
 * @param cmd control commands detailed in sensor_command_t
 * @param args control commands args
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 *     - ESP_ERR_NOT_SUPPORTED Function not supported on this sensor
 */
esp_err_t light_sensor_control(sensor_light_handle_t sensor, sensor_command_t cmd, void *args);

#ifdef __cplusplus
extern "C"
}
#endif

#endif
