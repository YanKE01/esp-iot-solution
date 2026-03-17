/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "sensor_hub_lp_detect.h"
#include "ulp_lp_core.h"
#include "ulp_lp_core_i2c.h"

#define LP_FAKE_HUMITURE_I2C_ADDR        0x68
#define LP_FAKE_HUMITURE_TEMP_REG        0x05
#define LP_FAKE_HUMITURE_HUMI_REG        0x08
#define LP_FAKE_HUMITURE_I2C_WAIT_TICKS  (-1)

typedef struct {
    esp_err_t (*init)(void *bus, uint8_t addr);
    esp_err_t (*deinit)(void);
    esp_err_t (*test)(void);
    esp_err_t (*acquire_humidity)(float *humidity);
    esp_err_t (*acquire_temperature)(float *temperature);
} sensor_hub_lp_humiture_impl_t;

static uint8_t s_lp_fake_humiture_addr = LP_FAKE_HUMITURE_I2C_ADDR;

static esp_err_t lp_fake_humiture_read_u8(uint8_t reg, uint8_t *value)
{
    return lp_core_i2c_master_write_read_device(LP_I2C_NUM_0,
                                                s_lp_fake_humiture_addr,
                                                &reg,
                                                sizeof(reg),
                                                value,
                                                1,
                                                LP_FAKE_HUMITURE_I2C_WAIT_TICKS);
}

esp_err_t lp_fake_humiture_init(bus_handle_t bus, uint8_t addr)
{
    (void)bus;
    s_lp_fake_humiture_addr = addr ? addr : LP_FAKE_HUMITURE_I2C_ADDR;
    return ESP_OK;
}

static esp_err_t lp_fake_humiture_deinit(void)
{
    return ESP_OK;
}

static esp_err_t lp_fake_humiture_test(void)
{
    return ESP_OK;
}

static esp_err_t lp_fake_humiture_acquire_humidity(float *humidity)
{
    uint8_t raw = 0;

    if (humidity == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = lp_fake_humiture_read_u8(LP_FAKE_HUMITURE_HUMI_REG, &raw);
    if (ret != ESP_OK) {
        return ret;
    }

    *humidity = (float)raw;
    return ESP_OK;
}

static esp_err_t lp_fake_humiture_acquire_temperature(float *temperature)
{
    uint8_t raw = 0;

    if (temperature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = lp_fake_humiture_read_u8(LP_FAKE_HUMITURE_TEMP_REG, &raw);
    if (ret != ESP_OK) {
        return ret;
    }

    *temperature = (float)raw;
    return ESP_OK;
}

static const sensor_hub_lp_humiture_impl_t s_lp_fake_humiture_impl = {
    .init = lp_fake_humiture_init,
    .deinit = lp_fake_humiture_deinit,
    .test = lp_fake_humiture_test,
    .acquire_humidity = lp_fake_humiture_acquire_humidity,
    .acquire_temperature = lp_fake_humiture_acquire_temperature,
};

SENSOR_HUB_LP_DETECT_FN(HUMITURE_ID, lp_fake_humiture, &s_lp_fake_humiture_impl);
