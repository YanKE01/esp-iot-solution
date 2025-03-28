/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <iostream>
#include "robotbox.hpp"

extern "C" void app_main(void)
{
    std::vector<robotbox::RevoluteDH> links = {
        robotbox::RevoluteDH(0.04145, 0.0, M_PI / 2, 0.0, 0.0),
        robotbox::RevoluteDH(0.0, -0.08285, 0.0, 0.0, 0.0),
        robotbox::RevoluteDH(0.0, -0.08285, 0.0, 0.0, 0.0),
        robotbox::RevoluteDH(0.0, 0.0, -M_PI / 2, 0.0, 0.0),
        robotbox::RevoluteDH(0.11, 0.0, 0.0, 0.0, 0.0),
    };
}
