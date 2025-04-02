/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <map>
#include <vector>
#include <argtable3/argtable3.h>
#include "servo.hpp"
#include "esp_console.h"
#include "app_wifi.hpp"

Servo *servo = nullptr;

static struct {
    struct arg_dbl *x;
    struct arg_dbl *y;
    struct arg_dbl *z;
    struct arg_end *end;
} motion_args;

extern std::vector<int> validate(double x, double y, double z, double alpha);

int cmd_do_motion(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &motion_args);
    if (nerrors > 0) {
        arg_print_errors(stderr, motion_args.end, argv[0]);
        return 1;
    }

    if (motion_args.x->count == 0 || motion_args.y->count == 0) {
        fprintf(stderr, "Error: both -x and -y parameters are required.\n");
        return 1;
    }

    double x = motion_args.x->dval[0];
    double y = motion_args.y->dval[0];
    double z = motion_args.z->dval[0];

    printf("Moving arm to position: x = %.2f, y = %.2f, z = %.2f\n", x, y, z);

    std::vector<int> result = validate(x, y, z, 180);

    if (!result.empty()) {
        servo->control(result, 1000);
    }

    return 0;
}

extern "C" void app_main(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    app_wifi_init();

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

    std::map<int, ServoConfig> servo_configs = {
        {1, {900, 3100, 0, 180, false}},
        {2, {900, 3100, 0, 180, true}},
        {3, {900, 3100, 0, 180, true}},
        {4, {900, 3100, 0, 180, true}},
        {5, {380, 3700, 0, 270, false}},
        {6, {900, 3100, 0, 180, false}},
    };
    servo = new Servo(UART_NUM_1, 24, 25, servo_configs);

    motion_args.x = arg_dbl0("x", NULL, "<x>", "X coordinate");
    motion_args.y = arg_dbl0("y", NULL, "<y>", "Y coordinate");
    motion_args.z = arg_dbl0("z", NULL, "<z>", "Z coordinate");
    motion_args.end = arg_end(3);

    const esp_console_cmd_t motion_cmd = {
        .command = "motion",
        .help = "move the arm",
        .hint = NULL,
        .func = cmd_do_motion,
        .argtable = &motion_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&motion_cmd));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
