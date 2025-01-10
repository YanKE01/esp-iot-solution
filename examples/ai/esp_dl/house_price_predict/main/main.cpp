/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <iostream>
#include <iomanip>
#include "esp_log.h"
#include "test_data.h"
#include "preprocess.h"
#include "dl_model_base.hpp"

static const char *TAG = "main";
using namespace dl;

std::pair<std::vector<std::vector<float>>, std::vector<float>> minMaxScale(
                                           const float data[][14], size_t rows,
                                           const float scaling_min[], const float scaling_max[]
                                       )
{
    std::vector<std::vector<float>> scaled_features;
    std::vector<float> labels;

    // Calculate the range values (max - min) for each feature
    std::vector<float> range_values(13); // Only for features, not including the label column
    for (size_t i = 0; i < 13; ++i) {
        range_values[i] = scaling_max[i] - scaling_min[i];
        if (range_values[i] == 0.0f) {
            range_values[i] = 1.0f; // Avoid division by zero
        }
    }

    // Apply Min-Max scaling to the features and extract labels
    for (size_t i = 0; i < rows; ++i) {
        std::vector<float> scaled_row;
        for (size_t j = 0; j < 13; ++j) { // Only scale the first 13 columns (features)
            float scaled_value = (data[i][j] - scaling_min[j]) / range_values[j];
            scaled_row.push_back(scaled_value);
        }
        scaled_features.push_back(scaled_row);
        labels.push_back(data[i][13]); // The last column is the label
    }

    return {scaled_features, labels};
}

int8_t quant_data[13] = {0};

extern "C" void app_main(void)
{
    Model *model = new Model("model", fbs::MODEL_LOCATION_IN_FLASH_PARTITION); /*!< Load model */
    if (model == nullptr) {
        ESP_LOGE(TAG, "Failed to create model");
        return;
    }

    printf("data size: %d\n", sizeof(test_data) / sizeof(test_data[0]));
    auto [scaled_features, labels] = minMaxScale(test_data, sizeof(test_data) / sizeof(test_data[0]), scaling_min, scaling_max);
    auto row = scaled_features[0];
    /*!< Load all map */
    std::map<std::string, TensorBase *> model_inputs;
    std::map<std::string, TensorBase *> graph_inputs = model->get_inputs();

    for (auto row : scaled_features) {
        for (auto graph_inputs_iter = graph_inputs.begin(); graph_inputs_iter != graph_inputs.end(); graph_inputs_iter++) {
            std::string input_name = graph_inputs_iter->first;
            TensorBase *input = graph_inputs_iter->second;

            if (input) {
                for (size_t i = 0; i < row.size(); ++i) {
                    quant_data[i] = quantize<int8_t>(row[i], 1.f / DL_SCALE(input->exponent));
                }

                for (int i = 0; i < 13; i++) {
                    printf("%d ", quant_data[i]);
                }

                TensorBase* model_input = new TensorBase({1, 13}, quant_data, input->exponent, input->dtype, false, MALLOC_CAP_SPIRAM);

                model_inputs.emplace(input_name, model_input);
            }
        }

        model->run(model_inputs);

        std::map<std::string, TensorBase *> outputs = model->get_outputs();
        for (const auto &output_pair : outputs) {
            TensorBase *output_tensor = output_pair.second;
            int8_t *infer_value_pointer = static_cast<int8_t *>(output_tensor->get_element_ptr());

            for (int i = 0; i < output_tensor->get_size(); ++i) {
                float scale = DL_SCALE(output_tensor->exponent);
                ESP_LOGI(TAG, "Predicted value: %.2f", dequantize(infer_value_pointer[i], scale));
            }
        }
    }

}
