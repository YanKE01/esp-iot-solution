/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "cv_detect.hpp"

#include <algorithm>
#include <vector>

#include "dl_image_process.hpp"
#include "esp_log.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>

constexpr const char *TAG = "cv_detect";

CvDetect::CvDetect(const Config &config) :
    m_config(config)
{
}

std::vector<CvDetect::Result> CvDetect::detect_gray(const uint8_t *image,
                                                    int image_width,
                                                    int image_height,
                                                    int detect_width,
                                                    int detect_height) const
{
    std::vector<Result> results;

    if (image == nullptr || image_width <= 0 || image_height <= 0) {
        ESP_LOGE(TAG, "Invalid input image");
        return results;
    }

    const float scale_x = static_cast<float>(image_width) / static_cast<float>(detect_width);
    const float scale_y = static_cast<float>(image_height) / static_cast<float>(detect_height);

    const cv::aruco::Dictionary dictionary =
        cv::aruco::getPredefinedDictionary(m_config.dictionary_id);
    cv::aruco::DetectorParameters detector_params;
    detector_params.cornerRefinementMethod = cv::aruco::CORNER_REFINE_SUBPIX;
    cv::aruco::ArucoDetector detector(dictionary, detector_params);

    std::vector<std::vector<cv::Point2f>> marker_corners;
    std::vector<int> marker_ids;
    cv::Mat detect_image(detect_height, detect_width, CV_8UC1, const_cast<uint8_t *>(image));
    detector.detectMarkers(detect_image, marker_corners, marker_ids);

    for (size_t i = 0; i < marker_ids.size(); ++i) {
        Result result = {};
        result.id = marker_ids[i];
        for (size_t corner = 0; corner < 4; ++corner) {
            result.corners[corner][0] = marker_corners[i][corner].x * scale_x;
            result.corners[corner][1] = marker_corners[i][corner].y * scale_y;
        }
        results.push_back(result);
    }

    ESP_LOGI(TAG, "Detected %u marker(s) from %dx%d gray image, detect size %dx%d",
             static_cast<unsigned>(results.size()), image_width, image_height, detect_width, detect_height);

    return results;
}

std::vector<CvDetect::Result> CvDetect::detect(const dl::image::img_t &image) const
{
    if (image.data == nullptr || image.width == 0 || image.height == 0) {
        ESP_LOGE(TAG, "Invalid esp-dl image");
        return {};
    }

    const int detect_width = std::min(m_config.detect_width, static_cast<int>(image.width));
    const int detect_height = std::min(m_config.detect_height, static_cast<int>(image.height));

    if (image.pix_type == dl::image::DL_IMAGE_PIX_TYPE_GRAY &&
            image.width == detect_width && image.height == detect_height) {
        return detect_gray(static_cast<const uint8_t *>(image.data),
                           image.width, image.height, detect_width, detect_height);
    }

    std::vector<uint8_t> gray_buffer(static_cast<size_t>(detect_width) * static_cast<size_t>(detect_height));
    dl::image::img_t detect_image = {
        .data = gray_buffer.data(),
                           .width = static_cast<uint16_t>(detect_width),
                           .height = static_cast<uint16_t>(detect_height),
                           .pix_type = dl::image::DL_IMAGE_PIX_TYPE_GRAY,
    };

    dl::image::ImageTransformer transformer;
    esp_err_t ret = transformer.set_src_img(image).set_dst_img(detect_image).transform();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to transform image from %s to gray detect image",
                 dl::image::pix_type2str(image.pix_type).c_str());
        return {};
    }

    return detect_gray(gray_buffer.data(), image.width, image.height, detect_width, detect_height);
}
