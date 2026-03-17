/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <string.h>
#include "esp_log.h"
#include "esp_amp.h"
#include "esp_amp_platform.h"
#include "sensor_hub_lp_detect.h"
#include "sensor_hub_amp_link.h"

static const char *TAG = "sensor_hub_lp";

typedef struct {
    const sensor_hub_lp_detect_fn_t *detect;
    bool initialized;
    int64_t next_poll_ms;
    uint32_t seq;
} sensor_hub_lp_runtime_t;

typedef struct {
    esp_err_t (*init)(void *bus, uint8_t addr);
    esp_err_t (*deinit)(void);
    esp_err_t (*test)(void);
    esp_err_t (*acquire_humidity)(float *humidity);
    esp_err_t (*acquire_temperature)(float *temperature);
} sensor_hub_lp_humiture_impl_t;

static sensor_hub_lp_cfg_t *s_lp_cfg_table;
static sensor_hub_lp_runtime_t s_lp_runtime[SENSOR_HUB_LP_MAX_SENSORS];
static esp_amp_queue_t s_lp_tx_queue;

static int sensor_hub_lp_queue_notify(void *args)
{
    (void)args;
    esp_amp_sw_intr_trigger(SENSOR_HUB_LP_SW_INTR_ID_QUEUE);
    return ESP_OK;
}

static const sensor_hub_lp_detect_fn_t *sensor_hub_lp_find_detect(const sensor_hub_lp_cfg_t *cfg)
{
    for (const sensor_hub_lp_detect_fn_t *p = __start_sensor_hub_lp_detect;
            p < __stop_sensor_hub_lp_detect; ++p) {
        if (p->sensor_type == cfg->type && strcmp(p->name, cfg->name) == 0) {
            return p;
        }
    }

    return NULL;
}

static esp_err_t sensor_hub_lp_sample_humiture(const sensor_hub_lp_cfg_t *cfg,
                                               sensor_hub_lp_runtime_t *runtime,
                                               sensor_hub_lp_data_pkt_t *pkt)
{
    const sensor_hub_lp_humiture_impl_t *impl = (const sensor_hub_lp_humiture_impl_t *)runtime->detect->impl;

    if (!runtime->initialized && impl->init) {
        assert(impl->init(NULL, cfg->addr) == ESP_OK);
        runtime->initialized = true;
    }

    memset(pkt, 0, sizeof(*pkt));
    pkt->type = cfg->type;
    pkt->addr = cfg->addr;
    pkt->timestamp = esp_amp_platform_get_time_ms();

    if (impl->acquire_temperature && impl->acquire_temperature(&pkt->data[0]) == ESP_OK) {
        pkt->valid_mask |= 0x01;
    }
    if (impl->acquire_humidity && impl->acquire_humidity(&pkt->data[1]) == ESP_OK) {
        pkt->valid_mask |= 0x02;
    }

    return pkt->valid_mask ? ESP_OK : ESP_FAIL;
}

int main(void)
{
    assert(esp_amp_init() == 0);
    assert(esp_amp_queue_sub_init(&s_lp_tx_queue, sensor_hub_lp_queue_notify, NULL, true, SENSOR_HUB_LP_SYS_INFO_ID_QUEUE) == 0);
    s_lp_cfg_table = esp_amp_sys_info_get(SENSOR_HUB_LP_SYS_INFO_ID_CFG, NULL, SYS_INFO_CAP_HP);
    assert(s_lp_cfg_table != NULL);
    esp_amp_event_notify(SENSOR_HUB_LP_EVENT_READY);

    while (1) {
        int64_t now_ms = esp_amp_platform_get_time_ms();

        for (uint8_t i = 0; i < SENSOR_HUB_LP_MAX_SENSORS; i++) {
            sensor_hub_lp_cfg_t *cfg = &s_lp_cfg_table[i];
            sensor_hub_lp_runtime_t *runtime = &s_lp_runtime[i];

            if (!cfg->active || !cfg->running) {
                runtime->next_poll_ms = now_ms;
                continue;
            }

            if (runtime->detect == NULL) {
                runtime->detect = sensor_hub_lp_find_detect(cfg);
                if (runtime->detect == NULL) {
                    ESP_LOGW(TAG, "lp detect sensor not found: %s", cfg->name);
                    continue;
                }
            }

            if (now_ms < runtime->next_poll_ms) {
                continue;
            }

            sensor_hub_lp_data_pkt_t sample_pkt = {0};
            esp_err_t ret = ESP_ERR_NOT_SUPPORTED;
            switch (cfg->type) {
            case HUMITURE_ID:
                ret = sensor_hub_lp_sample_humiture(cfg, runtime, &sample_pkt);
                break;
            default:
                break;
            }

            if (ret == ESP_OK) {
                sensor_hub_lp_data_pkt_t *pkt = NULL;
                if (esp_amp_queue_alloc_try(&s_lp_tx_queue, (void **)&pkt, sizeof(sensor_hub_lp_data_pkt_t)) == ESP_OK) {
                    sample_pkt.slot = i;
                    sample_pkt.seq = ++runtime->seq;
                    *pkt = sample_pkt;
                    assert(esp_amp_queue_send_try(&s_lp_tx_queue, pkt, sizeof(sensor_hub_lp_data_pkt_t)) == ESP_OK);
                } else {
                    ESP_LOGW(TAG, "lp queue alloc failed slot=%u", i);
                }
            } else {
                ESP_LOGW(TAG, "lp sample failed slot=%u ret=%s", i, esp_err_to_name(ret));
            }

            runtime->next_poll_ms = now_ms + cfg->min_delay_ms;
        }

        esp_amp_platform_delay_ms(10);
    }

    return 0;
}
