/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "cv_detect.hpp"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>

constexpr const char *TAG = "aruco_tracker";

extern "C" void app_main(void)
{
    constexpr int marker_id = 0;
    constexpr int marker_size = 200;

    const cv::aruco::Dictionary dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_50);

    cv::Mat marker;
    cv::aruco::generateImageMarker(dictionary, marker_id, marker_size, marker, 1);

    cv::Mat canvas(marker_size + 80, marker_size + 80, CV_8UC1, cv::Scalar(255));
    marker.copyTo(canvas(cv::Rect(40, 40, marker.cols, marker.rows)));

    void *detector_mem = heap_caps_malloc(sizeof(CvDetect), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (detector_mem == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate CvDetect in PSRAM");
        return;
    }

    CvDetect *detector = new(detector_mem) CvDetect({
        .dictionary_id = cv::aruco::DICT_4X4_50,
    });

    dl::image::img_t image = {
        .data = canvas.data,
        .width = static_cast<uint16_t>(canvas.cols),
        .height = static_cast<uint16_t>(canvas.rows),
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_GRAY,
    };

    std::vector<CvDetect::Result> results = detector->detect(image);

    ESP_LOGI(TAG, "Generated DICT_4X4_50 marker: id=%d, size=%dx%d, detected=%u",
             marker_id, marker.cols, marker.rows, static_cast<unsigned>(results.size()));

    for (const auto &result : results) {
        ESP_LOGI(TAG, "Marker ID %d", result.id);
        for (int i = 0; i < 4; ++i) {
            ESP_LOGI(TAG, "  corner[%d] = (%.1f, %.1f)", i,
                     result.corners[i][0], result.corners[i][1]);
        }
    }

    detector->~CvDetect();
    heap_caps_free(detector_mem);
}
