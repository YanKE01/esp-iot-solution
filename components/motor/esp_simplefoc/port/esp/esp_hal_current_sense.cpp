/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "current_sense/hardware_api.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_check.h"
#include "esp_log.h"
#include "soc/soc_caps.h"

static const char *TAG = "esp_hal_current_sense";
static constexpr adc_atten_t ADC_ATTENUATION = ADC_ATTEN_DB_12;
static constexpr int ADC_PIN_NOT_SET = static_cast<int>(NOT_SET);

struct EspAdcChannelConfig {
    int pin = ADC_PIN_NOT_SET;
    adc_unit_t unit = ADC_UNIT_1;
    adc_channel_t channel = ADC_CHANNEL_0;
    adc_cali_handle_t cali_handle = nullptr;
};

struct EspInlineCurrentSenseParams {
    int pins[3] = {ADC_PIN_NOT_SET, ADC_PIN_NOT_SET, ADC_PIN_NOT_SET};
    float adc_voltage_conv = 0.0f;
    EspAdcChannelConfig channels[3];
};

static adc_oneshot_unit_handle_t s_adc_unit_handles[SOC_ADC_PERIPH_NUM] = {};

static adc_oneshot_unit_handle_t get_adc_unit_handle(adc_unit_t unit)
{
    const int unit_index = unit - 1;
    if (unit_index < 0 || unit_index >= SOC_ADC_PERIPH_NUM) {
        return nullptr;
    }

    if (s_adc_unit_handles[unit_index] == nullptr) {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = unit,
            .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        if (adc_oneshot_new_unit(&init_config, &s_adc_unit_handles[unit_index]) != ESP_OK) {
            ESP_LOGE(TAG, "failed to create ADC unit handle for ADC%d", unit);
            return nullptr;
        }
    }

    return s_adc_unit_handles[unit_index];
}

static adc_cali_handle_t create_adc_cali_handle(adc_unit_t unit, adc_channel_t channel)
{
    adc_cali_handle_t handle = nullptr;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = ADC_ATTENUATION,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_config, &handle) == ESP_OK) {
        return handle;
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = ADC_ATTENUATION,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_config, &handle) == ESP_OK) {
        return handle;
    }
#endif

    return nullptr;
}

static EspAdcChannelConfig *find_adc_channel_config(const int pin, EspInlineCurrentSenseParams *params)
{
    for (int i = 0; i < 3; i++) {
        if (params->channels[i].pin == pin) {
            return &params->channels[i];
        }
    }

    return nullptr;
}

float _readADCVoltageInline(const int pin, const void *cs_params)
{
    if (cs_params == nullptr) {
        return 0.0f;
    }

    EspInlineCurrentSenseParams *params = (EspInlineCurrentSenseParams *)cs_params;
    EspAdcChannelConfig *channel_config = find_adc_channel_config(pin, params);
    if (channel_config == nullptr) {
        return 0.0f;
    }

    adc_oneshot_unit_handle_t adc_handle = get_adc_unit_handle(channel_config->unit);
    if (adc_handle == nullptr) {
        return 0.0f;
    }

    int voltage_mv = 0;
    if (channel_config->cali_handle == nullptr) {
        return 0.0f;
    }
    if (adc_oneshot_get_calibrated_result(adc_handle, channel_config->cali_handle, channel_config->channel, &voltage_mv) != ESP_OK) {
        return 0.0f;
    }

    // The SimpleFOC current sense layer expects voltage in volts, while ESP ADC calibration returns mV.
    return voltage_mv / 1000.0f;
}

void *_configureADCInline(const void *driver_params, const int pinA, const int pinB, const int pinC)
{
    _UNUSED(driver_params);

    EspInlineCurrentSenseParams *params = new EspInlineCurrentSenseParams;
    params->pins[0] = pinA;
    params->pins[1] = pinB;
    params->pins[2] = pinC;
    params->adc_voltage_conv = 3.3f / 4095.0f;

    const int pins[3] = {pinA, pinB, pinC};
    for (int i = 0; i < 3; i++) {
        if (!_isset(pins[i])) {
            continue;
        }

        adc_unit_t unit = ADC_UNIT_1;
        adc_channel_t channel = ADC_CHANNEL_0;
        if (adc_oneshot_io_to_channel(pins[i], &unit, &channel) != ESP_OK) {
            ESP_LOGE(TAG, "GPIO%d is not a valid ADC pin", pins[i]);
            delete params;
            return SIMPLEFOC_CURRENT_SENSE_INIT_FAILED;
        }

        adc_oneshot_unit_handle_t adc_handle = get_adc_unit_handle(unit);
        if (adc_handle == nullptr) {
            delete params;
            return SIMPLEFOC_CURRENT_SENSE_INIT_FAILED;
        }

        adc_oneshot_chan_cfg_t channel_config = {
            .atten = ADC_ATTENUATION,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        if (adc_oneshot_config_channel(adc_handle, channel, &channel_config) != ESP_OK) {
            ESP_LOGE(TAG, "failed to configure ADC channel for GPIO%d", pins[i]);
            delete params;
            return SIMPLEFOC_CURRENT_SENSE_INIT_FAILED;
        }

        params->channels[i].pin = pins[i];
        params->channels[i].unit = unit;
        params->channels[i].channel = channel;
        params->channels[i].cali_handle = create_adc_cali_handle(unit, channel);
        if (params->channels[i].cali_handle == nullptr) {
            ESP_LOGE(TAG, "failed to create ADC calibration handle for GPIO%d", pins[i]);
            delete params;
            return SIMPLEFOC_CURRENT_SENSE_INIT_FAILED;
        }
    }

    ESP_LOGI(TAG, "inline current sense configured on GPIOs: A=%d B=%d C=%d", pinA, pinB, pinC);
    return params;
}
