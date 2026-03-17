/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SENSOR_HUB_LP_DETECT_H_
#define _SENSOR_HUB_LP_DETECT_H_

#include <stddef.h>
#include "sensor_type.h"

typedef struct {
    const char *name;
    sensor_type_t sensor_type;
    const void *impl;
} sensor_hub_lp_detect_fn_t;

#define SENSOR_HUB_LP_DETECT_FN(type_id, name_id, impl_ptr) \
    static const sensor_hub_lp_detect_fn_t __attribute__((used)) \
        __attribute__((section("sensor_hub_lp_detect"))) sensor_hub_lp_detect_##name_id = { \
            .name = #name_id, \
            .sensor_type = (type_id), \
            .impl = (impl_ptr), \
        }

extern const sensor_hub_lp_detect_fn_t __start_sensor_hub_lp_detect[];
extern const sensor_hub_lp_detect_fn_t __stop_sensor_hub_lp_detect[];

static inline size_t sensor_hub_lp_detect_count(void)
{
    return (size_t)(__stop_sensor_hub_lp_detect - __start_sensor_hub_lp_detect);
}

#endif
