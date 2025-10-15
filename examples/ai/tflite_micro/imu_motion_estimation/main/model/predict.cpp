/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "predict.h"
#include "model_imu.h"
#include "imu.h"

static const char *TAG = "IMU_MODEL";

// Ring buffer parameters
#define WINDOW                  200               // Number of samples in window
#define FEATS                   3                 // gx, gy, gz
#define STRIDE                  20                // Stride for sliding window
#define RBUF_SAMPLES            WINDOW            // Ring buffer size: store recent WINDOW samples

// Custom ring buffer
static float ringbuf[RBUF_SAMPLES * FEATS];
static volatile uint32_t rb_head = 0;                     // [0, WINDOW)
static SemaphoreHandle_t rb_mutex = nullptr;
static SemaphoreHandle_t stride_sem = nullptr;            // Notify when stride is ready

static volatile uint32_t samples_since_last_infer = 0;

// Voting strategy
#define VOTE_WIN        9          // Voting window length (recent 9 predictions)
#define VOTE_MIN        4          // Minimum votes required for stable result
#define UNKNOWN_IDX     3          // Unknown class index

static int vote_buf[VOTE_WIN];
static uint32_t vote_head = 0;     // [0..VOTE_WIN)
static uint32_t vote_count = 0;    // Current valid count
static int last_announced = -1;    // Last announced class
static uint32_t last_announced_time = 0;  // Last announcement timestamp
static const char* kLabels[] = { "fling", "z", "fling_left", "unknown" };

// TensorFlow Lite model related
constexpr int kTensorArenaSize = 50000;
uint8_t tensor_arena[kTensorArenaSize];
tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;

void model_imu_init()
{
    // Create synchronization primitives
    rb_mutex = xSemaphoreCreateMutex();
    stride_sem = xSemaphoreCreateBinary();
    if (!rb_mutex || !stride_sem) {
        ESP_LOGE(TAG, "Create sync primitives failed");
        return;
    }

    const tflite::Model *model = tflite::GetModel(model_imu_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        MicroPrintf("Model provided is schema version %d not equal to supported "
                    "version %d.",
                    model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    static tflite::MicroMutableOpResolver<8> resolver;

    if (resolver.AddSoftmax() != kTfLiteOk) {
        MicroPrintf("Failed to add softmax operator.");
        return;
    }

    if (resolver.AddFullyConnected() != kTfLiteOk) {
        MicroPrintf("Failed to add fully connected operator.");
        return;
    }

    if (resolver.AddMaxPool2D() != kTfLiteOk) {
        MicroPrintf("Failed to add max pool 2D operator.");
        return;
    }

    if (resolver.AddConv2D() != kTfLiteOk) {
        MicroPrintf("Failed to add conv 2D operator.");
        return;
    }

    if (resolver.AddAveragePool2D() != kTfLiteOk) {
        MicroPrintf("Failed to add average pool 2D operator.");
        return;
    }

    if (resolver.AddExpandDims() != kTfLiteOk) {
        MicroPrintf("Failed to add expand dims operator.");
        return;
    }

    if (resolver.AddReshape() != kTfLiteOk) {
        MicroPrintf("Failed to add reshape operator.");
        return;
    }

    if (resolver.AddMean() != kTfLiteOk) {
        MicroPrintf("Failed to add mean operator.");
        return;
    }

    // Create interpreter
    static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // Allocate memory for model tensor
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        MicroPrintf("AllocateTensors() failed");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);
}

// Simple majority vote function - find the most frequent class (excluding unknown)
static int majority_vote(const int *buf, uint32_t n)
{
    if (n == 0) {
        return -1;
    }

    // Count votes for each class (excluding unknown)
    int counts[3] = {0};  // Only count: fling(0), z(1), fling_left(2)
    for (uint32_t i = 0; i < n; ++i) {
        int cls = buf[i];
        if (cls >= 0 && cls < UNKNOWN_IDX) {  // Exclude unknown class
            counts[cls]++;
        }
    }

    // Find class with maximum votes (excluding unknown)
    int best_cls = -1;
    int best_cnt = 0;
    for (int i = 0; i < 3; i++) {  // Only check first 3 classes
        if (counts[i] > best_cnt) {
            best_cnt = counts[i];
            best_cls = i;
        }
    }

    // Need minimum threshold for stability
    if (best_cnt < VOTE_MIN) {
        return -1;  // Not enough votes
    }

    return best_cls;
}

// Inference task: receives stride notification and performs model prediction
static void model_imu_infer_task(void *arg)
{
    static float window_buf[WINDOW * FEATS];

    while (true) {
        if (xSemaphoreTake(stride_sem, portMAX_DELAY) == pdTRUE) {
            // Copy recent WINDOW samples from ring buffer
            xSemaphoreTake(rb_mutex, portMAX_DELAY);
            uint32_t tail = rb_head;               // Oldest sample index
            uint32_t first_cnt = WINDOW - tail;    // [tail..WINDOW)
            memcpy(&window_buf[0],
                   &ringbuf[tail * FEATS],
                   first_cnt * FEATS * sizeof(float));
            if (tail > 0) {
                memcpy(&window_buf[first_cnt * FEATS],
                       &ringbuf[0],
                       tail * FEATS * sizeof(float));
            }
            xSemaphoreGive(rb_mutex);

            // Perform model inference
            if (interpreter && input && output) {
                // Copy window data to input tensor
                memcpy(input->data.f, window_buf, WINDOW * FEATS * sizeof(float));

                // Run inference
                TfLiteStatus invoke_status = interpreter->Invoke();
                if (invoke_status != kTfLiteOk) {
                    ESP_LOGE(TAG, "Model invoke failed");
                    continue;
                }

                // Get prediction result (assuming output is class probabilities)
                float *output_data = output->data.f;
                int num_classes = output->dims->data[1];

                // Find class with maximum probability
                int pred_idx = 0;
                float max_prob = output_data[0];
                for (int i = 1; i < num_classes; i++) {
                    if (output_data[i] > max_prob) {
                        max_prob = output_data[i];
                        pred_idx = i;
                    }
                }

                // Add prediction to voting buffer
                vote_buf[vote_head] = pred_idx;
                vote_head = (vote_head + 1) % VOTE_WIN;
                if (vote_count < VOTE_WIN) {
                    vote_count++;
                }

                // Perform majority vote
                int voted = majority_vote(vote_buf, vote_count);

                // Print result only when stable majority is formed
                if (voted >= 0) {
                    uint32_t current_time = xTaskGetTickCount();
                    // Print if different from last output or after 1 second
                    if (voted != last_announced || (current_time - last_announced_time) > pdMS_TO_TICKS(1500)) {
                        ESP_LOGI(TAG, "Final result: %s", kLabels[voted]);
                        last_announced = voted;
                        last_announced_time = current_time;
                    }
                }
            } else {
                ESP_LOGE(TAG, "Model not initialized");
            }
        }
    }
}

// Data acquisition task: 200Hz sampling, write to ring buffer, notify when stride is ready
void model_imu_feed_task(void *args)
{
    bmi270_handle_t bmi_handle = (bmi270_handle_t)args;
    if (bmi_handle == nullptr) {
        ESP_LOGE(TAG, "BMI270 handle is nullptr");
        return;
    }
    struct bmi2_sens_data sensor_data;
    int8_t rslt;

    while (true) {
        rslt = bmi2_get_sensor_data(&sensor_data, bmi_handle);
        if ((rslt == BMI2_OK) && (sensor_data.status & BMI2_DRDY_GYR)) {
            float gx = imu_lsb_to_dps(sensor_data.gyr.x, 2000.0f, bmi_handle->resolution);
            float gy = imu_lsb_to_dps(sensor_data.gyr.y, 2000.0f, bmi_handle->resolution);
            float gz = imu_lsb_to_dps(sensor_data.gyr.z, 2000.0f, bmi_handle->resolution);

            // Write to ring buffer (recent WINDOW samples)
            xSemaphoreTake(rb_mutex, portMAX_DELAY);
            uint32_t widx = rb_head * FEATS;
            ringbuf[widx + 0] = gx;
            ringbuf[widx + 1] = gy;
            ringbuf[widx + 2] = gz;
            rb_head = (rb_head + 1) % WINDOW;

            samples_since_last_infer = samples_since_last_infer + 1;
            if (samples_since_last_infer >= STRIDE) {
                samples_since_last_infer = 0;
                xSemaphoreGive(stride_sem);
            }
            xSemaphoreGive(rb_mutex);

        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void model_imu_predict_start(bmi270_handle_t bmi270)
{
    if (bmi270 == nullptr) {
        ESP_LOGE(TAG, "BMI270 handle is nullptr");
        return;
    }

    // Create data feed task
    TaskHandle_t feed_task_handle = NULL;
    xTaskCreate(model_imu_feed_task, "model_imu_feed_task", 5 * 1024, bmi270, 5, &feed_task_handle);
    if (feed_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create model_imu_feed_task");
        return;
    }

    // Create inference task
    TaskHandle_t infer_task_handle = NULL;
    xTaskCreate(model_imu_infer_task, "model_imu_infer_task", 8 * 1024, NULL, 4, &infer_task_handle);
    if (infer_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create model_imu_infer_task");
        return;
    }

    ESP_LOGI(TAG, "Model prediction started with feed and inference tasks");
}
