/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "i2c_bus.h"

void app_main(void)
{
    printf("Sensor Hub Test \n");
    unity_run_menu();
}
