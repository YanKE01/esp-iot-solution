/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <vector>

#include "dl_image_define.hpp"
#include <opencv2/objdetect/aruco_detector.hpp>

class CvDetect {
public:
    struct Config {
        int detect_width = 224;
        int detect_height = 224;
        cv::aruco::PredefinedDictionaryType dictionary_id;
    };

    struct Result {
        int id;
        float corners[4][2];
    };

    explicit CvDetect(const Config &config);

    std::vector<Result> detect(const dl::image::img_t &image) const;

private:
    std::vector<Result> detect_gray(const uint8_t *image,
                                    int image_width,
                                    int image_height,
                                    int detect_width,
                                    int detect_height) const;

    Config m_config;
};
