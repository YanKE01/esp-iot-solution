/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "net.h"

Net::Net(const char* model_name)
{
    m_model = new dl::Model(model_name, fbs::MODEL_LOCATION_IN_FLASH_PARTITION);
}

Net::~Net()
{
    if (m_model) {
        delete m_model;
        m_model = nullptr;
    }
}

float Net::run(std::vector<float> input)
{

    return -1.0f;
}
