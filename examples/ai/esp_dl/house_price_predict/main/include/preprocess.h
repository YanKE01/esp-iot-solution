/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include <limits>
#include <iostream>

class MaxMinScaler {
public:
    MaxMinScaler(float feature_min = 0.0f, float feature_max = 1.0f);

    std::vector<std::vector<float>> fit_transform(const std::vector<std::vector<float>> &features);

private:
    float feature_min_;
    float feature_max_;
};

void split_features_and_labels(const float data[][14], size_t num_rows,
                               std::vector<std::vector<float>> &features,
                               std::vector<float> &labels);
