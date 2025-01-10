/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "preprocess.h"
#include <algorithm>

void split_features_and_labels(const float data[][14], size_t num_rows,
                               std::vector<std::vector<float>> &features,
                               std::vector<float> &labels)
{
    features.clear();
    labels.clear();

    for (size_t i = 0; i < num_rows; ++i) {
        std::vector<float> row_features;

        for (size_t j = 0; j < 13; ++j) {
            row_features.push_back(data[i][j]);
        }

        features.push_back(row_features);
        labels.push_back(data[i][13]);
    }
}

MaxMinScaler::MaxMinScaler(float feature_min, float feature_max)
    : feature_min_(feature_min), feature_max_(feature_max) {}

// 计算每个特征的最小值和最大值并进行缩放
std::vector<std::vector<float>> MaxMinScaler::fit_transform(const std::vector<std::vector<float>> &features)
{
    if (features.empty() || features[0].empty()) {
        printf("Input features are empty.\n");
        return {};
    }

    size_t num_samples = features.size();
    size_t num_features = features[0].size();

    printf("num_samples: %d, num_features: %d\n", num_samples, num_features);

    // 初始化每个特征的最小值和最大值
    std::vector<float> min(num_features);
    std::vector<float> max(num_features);

    // 计算每个特征的最小值和最大值
    for (const auto &row : features) {
        auto [min_it, max_it] = std::minmax_element(row.begin(), row.end());
        min.push_back(*min_it);
        max.push_back(*max_it);
    }

    // for (const auto &row : features) {
    //     for (size_t j = 0; j < num_features; ++j) {
    //         min[j] = std::min(min[j], row[j]);
    //         max[j] = std::max(max[j], row[j]);
    //     }

    //     for(int i=0;i<row.size();i++) {
    //         printf("%.5f ",row[i]);
    //     }
    //     printf("\n");
    // }

    // for(int i=0;i<min.size();i++) {
    //     printf("min: %.5f, max: %.5f\n", min[i], max[i]);
    // }

    // 创建缩放后的数据
    std::vector<std::vector<float>> scaled_features(num_samples, std::vector<float>(num_features));

    // 标准化并缩放数据
    for (size_t i = 0; i < num_samples; ++i) {
        for (size_t j = 0; j < num_features; ++j) {
            if (max[j] == min[j]) {
                scaled_features[i][j] = feature_min_;  // 特殊情况：所有值相等，直接赋值为 feature_min_
            } else {
                float X_std = (features[i][j] - min[j]) / (max[j] - min[j]);  // 标准化
                scaled_features[i][j] = X_std * (feature_max_ - feature_min_) + feature_min_;  // 缩放
            }
        }
    }

    return scaled_features;
}
