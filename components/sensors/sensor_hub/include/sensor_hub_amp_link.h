/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "sensor_type.h"
#include "esp_amp_sw_intr.h"

#define SENSOR_HUB_LP_EVENT_READY      (1 << 0)
#define SENSOR_HUB_LP_MAX_SENSORS      4
#define SENSOR_HUB_LP_NAME_MAX_LEN     32
#define SENSOR_HUB_LP_QUEUE_LEN        8

#define SENSOR_HUB_LP_SYS_INFO_ID_CFG   0x5348
#define SENSOR_HUB_LP_SYS_INFO_ID_QUEUE 0x5349
#define SENSOR_HUB_LP_SW_INTR_ID_QUEUE  SW_INTR_ID_15

typedef struct {
    uint8_t active;
    uint8_t running;
    uint8_t addr;
    uint8_t reserved0;
    sensor_type_t type;
    uint32_t min_delay_ms;
    char name[SENSOR_HUB_LP_NAME_MAX_LEN];
} sensor_hub_lp_cfg_t;

typedef struct {
    uint8_t slot;
    uint8_t addr;
    uint8_t valid_mask;
    uint8_t reserved0;
    sensor_type_t type;
    uint32_t seq;
    int64_t timestamp;
    float data[4];
} sensor_hub_lp_data_pkt_t;
