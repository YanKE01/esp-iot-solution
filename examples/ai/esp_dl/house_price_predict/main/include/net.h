/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include "dl_model_base.hpp"

class Net {
public:
    Net(const char* model_name);
    ~Net();
    float run(std::vector<float> input);
private:
    dl::Model *m_model;
};
