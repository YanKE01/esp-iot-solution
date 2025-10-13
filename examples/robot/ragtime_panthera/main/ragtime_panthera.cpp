/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <iostream>
#include <cmath>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "matrix.hpp"
#include "robotics.hpp"
#include "dmbot.hpp"

static const char *TAG = "Ragtime_Panthera";

robotics::Serial_Link<6> *panther6dof = NULL;
damiao::Motor_Control *dm = NULL;
damiao::Motor *M1 = NULL;
damiao::Motor *M2 = NULL;
damiao::Motor *M3 = NULL;
damiao::Motor *M4 = NULL;
damiao::Motor *M5 = NULL;
damiao::Motor *M6 = NULL;
damiao::Motor *M7 = NULL;

static EventGroupHandle_t panthera_event_group = NULL;

// Event bits
#define PANTHERA_RANGE_BIT    BIT0
#define IKINE_TEST_BIT        BIT1
#define MOVE_PANTHERA_BIT     BIT2
#define PANTHERA_GRIPPER_BIT  BIT3
#define PANTHERA_REPLAY_BIT   BIT4
#define ALL_EVENTS_BIT        (PANTHERA_RANGE_BIT | IKINE_TEST_BIT | MOVE_PANTHERA_BIT | PANTHERA_GRIPPER_BIT | PANTHERA_REPLAY_BIT)

static struct {
    struct arg_str *enable;
    struct arg_end *end;
} panthera_enable_args;

static struct {
    struct arg_dbl *x;
    struct arg_dbl *y;
    struct arg_dbl *z;
    struct arg_end *end;
} move_panthera_args;

static struct {
    struct arg_dbl *angle;
    struct arg_end *end;
} gripper_control_args;

static struct {
    struct arg_int *x;
    struct arg_int *y;
    struct arg_end *end;
} panthera_gripper_args;

// Global variables to store move target position
static double move_target_x = 0.0;
static double move_target_y = 0.0;
static double move_target_z = 0.0;

// Global variables to store pixel coordinates
static int pixel_x = 0;
static int pixel_y = 0;

int enable_panthera_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &panthera_enable_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, panthera_enable_args.end, argv[0]);
        return 1;
    }

    if (panthera_enable_args.enable->count > 0) {
        const char *value = panthera_enable_args.enable->sval[0];
        if (strcmp(value, "true") == 0) {
            ESP_LOGI(TAG, "Panthera enabled");
            dm->enable(*M1);
            dm->enable(*M2);
            dm->enable(*M3);
            dm->enable(*M4);
            dm->enable(*M5);
            dm->enable(*M6);
            dm->enable(*M7);
        } else if (strcmp(value, "false") == 0) {
            ESP_LOGI(TAG, "Panthera disabled");
            dm->disable(*M1);
            dm->disable(*M2);
            dm->disable(*M3);
            dm->disable(*M4);
            dm->disable(*M5);
            dm->disable(*M6);
            dm->disable(*M7);
        } else {
            ESP_LOGE(TAG, "Invalid value for panthera_enable: %s (use 'true' or 'false')", value);
            return 1;
        }
    } else {
        ESP_LOGE(TAG, "Missing argument: use 'panthera_enable true' or 'panthera_enable false'");
        return 1;
    }

    return 0;
}

int home_panthera_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Moving panthera to home position");

    // Home position: all joints at 0
    float home_q[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    // Move all motors to home position
    dm->control_pos_vel(*M1, home_q[0], 2.0f);
    dm->control_pos_vel(*M2, home_q[1], 2.0f);
    dm->control_pos_vel(*M3, -home_q[2], 2.0f);  // Note: M3 direction is inverted
    dm->control_pos_vel(*M4, home_q[3], 2.0f);
    dm->control_pos_vel(*M5, home_q[4], 2.0f);
    dm->control_pos_vel(*M6, home_q[5], 2.0f);

    ESP_LOGI(TAG, "Home position command sent");
    return 0;
}

int panthera_range_test_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Triggering range test via event");

    if (panthera_event_group == NULL) {
        ESP_LOGE(TAG, "Event group not initialized");
        return 1;
    }

    // Set the PANTHERA_RANGE event bit
    xEventGroupSetBits(panthera_event_group, PANTHERA_RANGE_BIT);

    ESP_LOGI(TAG, "Range test event triggered");
    return 0;
}

int ikine_test_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Triggering ikine test via event");

    if (panthera_event_group == NULL) {
        ESP_LOGE(TAG, "Event group not initialized");
        return 1;
    }

    // Set the IKINE_TEST event bit
    xEventGroupSetBits(panthera_event_group, IKINE_TEST_BIT);

    ESP_LOGI(TAG, "Ikine test event triggered");
    return 0;
}

int move_panthera_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &move_panthera_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, move_panthera_args.end, argv[0]);
        return 1;
    }

    if (move_panthera_args.x->count > 0 &&
            move_panthera_args.y->count > 0 &&
            move_panthera_args.z->count > 0) {

        // Store target position in global variables
        move_target_x = move_panthera_args.x->dval[0];
        move_target_y = move_panthera_args.y->dval[0];
        move_target_z = move_panthera_args.z->dval[0];

        ESP_LOGI(TAG, "Triggering move to position: x=%.3f, y=%.3f, z=%.3f",
                 move_target_x, move_target_y, move_target_z);

        if (panthera_event_group == NULL) {
            ESP_LOGE(TAG, "Event group not initialized");
            return 1;
        }

        // Set the MOVE_PANTHERA event bit
        xEventGroupSetBits(panthera_event_group, MOVE_PANTHERA_BIT);

        ESP_LOGI(TAG, "Move event triggered");
    } else {
        ESP_LOGE(TAG, "Missing arguments: use 'panthera_move <x> <y> <z>'");
        return 1;
    }

    return 0;
}

int zero_panthera_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Zeroing panthera");
    dm->set_zero_pos(*M1);
    dm->set_zero_pos(*M2);
    dm->set_zero_pos(*M3);
    dm->set_zero_pos(*M4);
    dm->set_zero_pos(*M5);
    dm->set_zero_pos(*M6);
    dm->set_zero_pos(*M7);
    return 0;
}

int gripper_control_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &gripper_control_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, gripper_control_args.end, argv[0]);
        return 1;
    }

    if (gripper_control_args.angle->count > 0) {
        double angle = gripper_control_args.angle->dval[0];
        ESP_LOGI(TAG, "Controlling gripper to angle: %.3f radians", angle);

        // Control gripper motor (M7) to the specified angle (already in radians)
        dm->control_pos_vel(*M7, angle, 5.0f);

        ESP_LOGI(TAG, "Gripper control command sent");
    } else {
        ESP_LOGE(TAG, "Missing argument: use 'gripper_control <angle>'");
        return 1;
    }

    return 0;
}

int panthera_gripper_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &panthera_gripper_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, panthera_gripper_args.end, argv[0]);
        return 1;
    }

    if (panthera_gripper_args.x->count > 0 && panthera_gripper_args.y->count > 0) {
        // 获取像素坐标并存储到全局变量
        pixel_x = panthera_gripper_args.x->ival[0];
        pixel_y = panthera_gripper_args.y->ival[0];

        ESP_LOGI(TAG, "Pixel coordinates stored: x=%d, y=%d", pixel_x, pixel_y);

        if (panthera_event_group == NULL) {
            ESP_LOGE(TAG, "Event group not initialized");
            return 1;
        }

        // 发送事件给 panthera_cmd_task
        xEventGroupSetBits(panthera_event_group, PANTHERA_GRIPPER_BIT);

        ESP_LOGI(TAG, "Panthera gripper event triggered");
    } else {
        ESP_LOGE(TAG, "Missing arguments: use 'panthera_gripper <x> <y>' where x and y are pixel coordinates");
        return 1;
    }

    return 0;
}

int panthera_replay_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Replay the panthera movement");

    if (panthera_event_group == NULL) {
        ESP_LOGE(TAG, "Event group not initialized");
        return 1;
    }

    // Set the PANTHERA_REPLAY event bit
    xEventGroupSetBits(panthera_event_group, PANTHERA_REPLAY_BIT);
    return 0;
}

void panthera_cmd_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Panthera command task started");

    // Perform ikine test here
    float q[6] = {0.0, DEG2RAD(43.0), DEG2RAD(-45.0), DEG2RAD(63.0), 0.0, 0.0};
    Matrixf<4, 4> T = panther6dof->fkine(q);
    matrix_print::print(T, "Starting Position Forward Kinematics Result T");

    double calib_matrix[3][3] = {
        {-3.40250094e-03,  1.36584364e-04,  1.12981715e+00},
        {-1.19690298e-04,  3.08720837e-03, -8.49403807e-01},
        {5.04189175e-07, -1.35416482e-08,  5.25599177e-02}
    };

    double replay_points[][7] = {
        {-0.0, 1.89345383644104, 0.3576333224773407, -1.358243703842163, -0.025749599561095238, -0.02994582988321781, 4.020561695098877},
        {-0.0, 1.89345383644104, 0.3576333224773407, -1.358243703842163, -0.025749599561095238, -0.02994582988321781, 3.076409578323364},
        {0.0, 0.11043716967105865, 0.02040894143283367, -0.15812161564826965, -0.010490577667951584, -0.07877469807863235, 3.0638208389282227}
    };

    while (1) {
        // Wait for any event (PANTHERA_RANGE, IKINE_TEST, MOVE_PANTHERA, or PANTHERA_GRIPPER)
        EventBits_t bits = xEventGroupWaitBits(
                               panthera_event_group,
                               PANTHERA_RANGE_BIT | IKINE_TEST_BIT | MOVE_PANTHERA_BIT | PANTHERA_GRIPPER_BIT | PANTHERA_REPLAY_BIT,
                               pdTRUE,  // Clear bits on exit
                               pdFALSE, // Wait for any bit
                               portMAX_DELAY
                           );

        if (bits & PANTHERA_RANGE_BIT) {
            ESP_LOGI(TAG, "PANTHERA_RANGE event received, starting range test");

            dm->control_pos_vel(*M1,  q[0], 10.0f);
            dm->control_pos_vel(*M2,  q[1], 10.0f);
            dm->control_pos_vel(*M3,  -q[2], 10.0f);
            dm->control_pos_vel(*M4,  q[3], 10.0f);
            dm->control_pos_vel(*M5,  q[4], 10.0f);
            dm->control_pos_vel(*M6,  q[5], 10.0f);
            dm->control_pos_vel(*M7,  0.0f, 10.0f);
            vTaskDelay(pdMS_TO_TICKS(5 * 1000));

            // Test specific points
            int test_points[5][2] = {{23, 15}, {23, 0}, {23, -15}, {0, -15}, {0, 0}};

            for (int i = 0; i < 5; i++) {
                int dx = test_points[i][0];
                int dy = test_points[i][1];

                // Create target transformation matrix with offset
                Matrixf<4, 4> T1 = T;
                T1[0][3] = T[0][3] + dx / 100.0f;  // x offset in cm
                T1[1][3] = T[1][3] + dy / 100.0f;  // y offset in cm

                // Perform inverse kinematics
                Matrixf<6, 1> q_ikine = panther6dof->ikine(T1, Matrixf<6, 1>(q));

                // Perform forward kinematics with solved joint angles
                Matrixf<4, 4> T2 = panther6dof->fkine(q_ikine);

                // Calculate position errors
                float error_x = T1[0][3] - T2[0][3];
                float error_y = T1[1][3] - T2[1][3];

                // Print joint angles in radians
                std::cout << "Point " << i + 1 << " (dx=" << dx / 100.0f << ", dy=" << dy / 100.0f << ") - Radians: ";
                for (int j = 0; j < 6; j++) {
                    std::cout << q_ikine[j][0];
                    if (j < 5) {
                        std::cout << ", ";
                    }
                }
                std::cout << std::endl;

                // Print joint angles in degrees
                std::cout << "Point " << i + 1 << " (dx=" << dx / 100.0f << ", dy=" << dy / 100.0f << ") - Degrees: ";
                for (int j = 0; j < 6; j++) {
                    std::cout << q_ikine[j][0] * 180.0f / PI;
                    if (j < 5) {
                        std::cout << ", ";
                    }
                }
                std::cout << " deg" << std::endl;

                // Print errors
                std::cout << "Point " << i + 1 << " - Errors: error_x=" << error_x << ", error_y=" << error_y << std::endl;
                matrix_print::print(panther6dof->fkine(q_ikine), "q_ikine");
                std::cout << "----------------------------------------" << std::endl;

                dm->control_pos_vel(*M1, q_ikine[0][0], 10.0f);
                dm->control_pos_vel(*M2, q_ikine[1][0], 10.0f);
                dm->control_pos_vel(*M3, -q_ikine[2][0], 10.0f);
                dm->control_pos_vel(*M4, q_ikine[3][0], 10.0f);
                dm->control_pos_vel(*M5, q_ikine[4][0], 10.0f);
                dm->control_pos_vel(*M6, q_ikine[5][0], 10.0f);
                dm->control_pos_vel(*M7, 0.0f, 10.0f);
                vTaskDelay(pdMS_TO_TICKS(5 * 1000));
            }
        }

        if (bits & IKINE_TEST_BIT) {
            ESP_LOGI(TAG, "IKINE_TEST event received, starting ikine test");

            for (int dx = 0; dx <= 23; dx++) {
                for (int dy = -15; dy <= 15; dy++) {
                    Matrixf<4, 4> T1 = T;
                    T1[0][3] = T[0][3] + dx / 100.0f;  // x offset in cm
                    T1[1][3] = T[1][3] + dy / 100.0f;  // y offset in cm

                    // Perform inverse kinematics
                    Matrixf<6, 1> q_ikine = panther6dof->ikine(T1, Matrixf<6, 1>(q));

                    // Perform forward kinematics with solved joint angles
                    Matrixf<4, 4> T2 = panther6dof->fkine(q_ikine);

                    // Calculate position errors
                    float error_x = T1[0][3] - T2[0][3];
                    float error_y = T1[1][3] - T2[1][3];

                    // Only print if error is greater than 0.01
                    if (fabs(error_x) > 0.01 || fabs(error_y) > 0.01) {
                        std::cout << "*** ANOMALY DETECTED ***" << std::endl;

                        // Print joint angles in degrees
                        std::cout << "Joint angles: [";
                        for (int j = 0; j < 6; j++) {
                            std::cout << q_ikine[j][0] * 180.0f / PI;
                            if (j < 5) {
                                std::cout << ", ";
                            }
                        }
                        std::cout << "] deg" << std::endl;

                        // Print target and actual positions with errors
                        std::cout << "  Target: x=" << T1[0][3] << " y=" << T1[1][3] << std::endl;
                        std::cout << "  Actual: x=" << T2[0][3] << " y=" << T2[1][3] << std::endl;
                        std::cout << "  Error:  x=" << error_x << " y=" << error_y << std::endl;
                        std::cout << "----------------------------------------" << std::endl;
                    }

                    vTaskDelay(pdMS_TO_TICKS(1));
                }
            }

            ESP_LOGI(TAG, "Ikine test completed");
        }

        if (bits & MOVE_PANTHERA_BIT) {
            ESP_LOGI(TAG, "MOVE_PANTHERA event received, moving to position: x=%.3f, y=%.3f, z=%.3f",
                     move_target_x, move_target_y, move_target_z);

            // Create target transformation matrix with x, y, z position
            Matrixf<4, 4> T1 = T;
            T1[0][3] = move_target_x;
            T1[1][3] = move_target_y;
            T1[2][3] = move_target_z;

            // Perform inverse kinematics
            Matrixf<6, 1> q_ikine = panther6dof->ikine(T1, Matrixf<6, 1>(q));

            // Perform forward kinematics with solved joint angles
            Matrixf<4, 4> T2 = panther6dof->fkine(q_ikine);

            // Calculate position errors
            float error_x = T1[0][3] - T2[0][3];
            float error_y = T1[1][3] - T2[1][3];
            float error_z = T1[2][3] - T2[2][3];

            // Print joint angles in degrees
            std::cout << "Degrees: ";
            for (int j = 0; j < 6; j++) {
                std::cout << q_ikine[j][0] * 180.0f / PI;
                if (j < 5) {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;

            // Print errors
            std::cout << "Errors: error_x=" << error_x << ", error_y=" << error_y << ", error_z=" << error_z << std::endl;
            matrix_print::print(panther6dof->fkine(q_ikine), "q_ikine");
            std::cout << "----------------------------------------" << std::endl;

            if (fabs(error_x) > 0.01 || fabs(error_y) > 0.01 || fabs(error_z) > 0.01) {
                ESP_LOGE(TAG, "Move command failed, errors are too large");
                continue;
            }

            dm->control_pos_vel(*M1, q_ikine[0][0], 10.0f);
            dm->control_pos_vel(*M2, q_ikine[1][0], 10.0f);
            dm->control_pos_vel(*M3, -q_ikine[2][0], 10.0f);
            dm->control_pos_vel(*M4, q_ikine[3][0], 10.0f);
            dm->control_pos_vel(*M5, q_ikine[4][0], 10.0f);
            dm->control_pos_vel(*M6, q_ikine[5][0], 10.0f);

            ESP_LOGI(TAG, "Move command completed");
        }

        if (bits & PANTHERA_GRIPPER_BIT) {
            ESP_LOGI(TAG, "PANTHERA_GRIPPER event received, processing pixel coordinates: x=%d, y=%d", pixel_x, pixel_y);
            double a[3] = {pixel_x * 1.0f, pixel_y * 1.0f, 1.0f};
            double b[3] = {0.0, 0.0, 0.0};
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    b[i] += calib_matrix[i][j] * a[j];
                }
            }
            ESP_LOGI(TAG, "Pixel coordinates converted to world coordinates: x=%f, y=%f, z=%f", b[0], b[1], b[2]);

            dm->control_pos_vel(*M7, 4.2f, 10.0f);

            // Create target transformation matrix with x, y, z position
            Matrixf<4, 4> T1 = T;
            T1[0][3] = b[0];
            T1[1][3] = b[1];
            T1[2][3] = b[2];

            // Perform inverse kinematics
            Matrixf<6, 1> q_ikine = panther6dof->ikine(T1, Matrixf<6, 1>(q));

            // Perform forward kinematics with solved joint angles
            Matrixf<4, 4> T2 = panther6dof->fkine(q_ikine);

            // Calculate position errors
            float error_x = T1[0][3] - T2[0][3];
            float error_y = T1[1][3] - T2[1][3];
            float error_z = T1[2][3] - T2[2][3];

            // Print joint angles in degrees
            std::cout << "Degrees: ";
            for (int j = 0; j < 6; j++) {
                std::cout << q_ikine[j][0] * 180.0f / PI;
                if (j < 5) {
                    std::cout << ", ";
                }
            }
            std::cout << std::endl;

            // Print errors
            std::cout << "Errors: error_x=" << error_x << ", error_y=" << error_y << ", error_z=" << error_z << std::endl;
            matrix_print::print(panther6dof->fkine(q_ikine), "q_ikine");
            std::cout << "----------------------------------------" << std::endl;

            if (fabs(error_x) > 0.01 || fabs(error_y) > 0.01 || fabs(error_z) > 0.01) {
                ESP_LOGE(TAG, "Move command failed, errors are too large");
                continue;
            }

            dm->control_pos_vel(*M1, q_ikine[0][0], 10.0f);
            dm->control_pos_vel(*M2, q_ikine[1][0], 10.0f);
            dm->control_pos_vel(*M3, -q_ikine[2][0], 10.0f);
            dm->control_pos_vel(*M4, q_ikine[3][0], 10.0f);
            dm->control_pos_vel(*M5, q_ikine[4][0], 10.0f);
            dm->control_pos_vel(*M6, q_ikine[5][0], 10.0f);

            ESP_LOGI(TAG, "Downing the gripper");
            vTaskDelay(pdMS_TO_TICKS(2000));
            T1[0][3] = b[0];
            T1[1][3] = b[1];
            T1[2][3] = 0.01f;
            q_ikine = panther6dof->ikine(T1, Matrixf<6, 1>(q));
            T2 = panther6dof->fkine(q_ikine);

            error_x = T1[0][3] - T2[0][3];
            error_y = T1[1][3] - T2[1][3];
            error_z = T1[2][3] - T2[2][3];

            std::cout << "Errors: error_x=" << error_x << ", error_y=" << error_y << ", error_z=" << error_z << std::endl;
            matrix_print::print(panther6dof->fkine(q_ikine), "q_ikine");
            std::cout << "----------------------------------------" << std::endl;

            if (fabs(error_x) > 0.01 || fabs(error_y) > 0.01 || fabs(error_z) > 0.01) {
                ESP_LOGE(TAG, "Downing the gripper failed, errors are too large");
                continue;
            }

            dm->control_pos_vel(*M1, q_ikine[0][0], 10.0f);
            dm->control_pos_vel(*M2, q_ikine[1][0], 10.0f);
            dm->control_pos_vel(*M3, -q_ikine[2][0], 10.0f);
            dm->control_pos_vel(*M4, q_ikine[3][0], 10.0f);
            dm->control_pos_vel(*M5, q_ikine[4][0], 10.0f);
            dm->control_pos_vel(*M6, q_ikine[5][0], 10.0f);
            ESP_LOGI(TAG, "Downing the gripper completed");

            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGI(TAG, "Gripper object");
            dm->control_pos_vel(*M7, 1.42f, 10.0f);

            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGI(TAG, "Go Home");
            dm->control_pos_vel(*M1, 0.0f, 2.0f);
            dm->control_pos_vel(*M2, 0.0f, 2.0f);
            dm->control_pos_vel(*M3, 0.0f, 2.0f);
            dm->control_pos_vel(*M4, 0.0f, 2.0f);
            dm->control_pos_vel(*M5, 0.0f, 2.0f);
            dm->control_pos_vel(*M6, 0.0f, 2.0f);
        }

        if (bits & PANTHERA_REPLAY_BIT) {
            ESP_LOGI(TAG, "PANTHERA_REPLAY event received, starting replay sequence");

            // 定义位置容差 (弧度)
            const float position_tolerance = 0.05f; // 约3度
            const float velocity = 5.0f; // 5 rad/s

            // 获取replay_points数组大小
            int num_points = sizeof(replay_points) / sizeof(replay_points[0]);

            for (int point_idx = 0; point_idx < num_points; point_idx++) {
                ESP_LOGI(TAG, "Executing replay point %d/%d", point_idx + 1, num_points);

                // 发送目标位置到所有关节 (前6个是机器人关节，第7个是夹爪)
                dm->control_pos_vel(*M1, static_cast<float>(replay_points[point_idx][0]), velocity);
                dm->control_pos_vel(*M2, static_cast<float>(replay_points[point_idx][1]), velocity);
                dm->control_pos_vel(*M3, static_cast<float>(replay_points[point_idx][2]), velocity);
                dm->control_pos_vel(*M4, static_cast<float>(replay_points[point_idx][3]), velocity);
                dm->control_pos_vel(*M5, static_cast<float>(replay_points[point_idx][4]), velocity);
                dm->control_pos_vel(*M6, static_cast<float>(replay_points[point_idx][5]), velocity);
                dm->control_pos_vel(*M7, static_cast<float>(replay_points[point_idx][6]), velocity);

                // 等待所有关节到达目标位置
                bool all_joints_reached = false;
                int timeout_counter = 0;
                const int max_timeout = 1000; // 最大等待时间 (50ms * 1000 = 50秒)

                while (!all_joints_reached && timeout_counter < max_timeout) {
                    all_joints_reached = true;

                    // 检查每个关节是否到达目标位置
                    float current_positions[7] = {
                        M1->motor_fb_param.position,
                        M2->motor_fb_param.position,
                        M3->motor_fb_param.position,
                        M4->motor_fb_param.position,
                        M5->motor_fb_param.position,
                        M6->motor_fb_param.position,
                        M7->motor_fb_param.position
                    };

                    float target_positions[7] = {
                        static_cast<float>(replay_points[point_idx][0]),
                        static_cast<float>(replay_points[point_idx][1]),
                        static_cast<float>(replay_points[point_idx][2]),
                        static_cast<float>(replay_points[point_idx][3]),
                        static_cast<float>(replay_points[point_idx][4]),
                        static_cast<float>(replay_points[point_idx][5]),
                        static_cast<float>(replay_points[point_idx][6])
                    };

                    for (int joint = 0; joint < 7; joint++) {
                        float position_error = fabs(current_positions[joint] - target_positions[joint]);
                        if (position_error > position_tolerance) {
                            all_joints_reached = false;
                            break;
                        }
                    }

                    if (!all_joints_reached) {
                        vTaskDelay(pdMS_TO_TICKS(50)); // 等待50ms后再次检查
                        timeout_counter++;
                    }
                }

                if (all_joints_reached) {
                    ESP_LOGI(TAG, "Point %d reached successfully", point_idx + 1);
                } else {
                    ESP_LOGW(TAG, "Point %d timeout - some joints may not have reached target", point_idx + 1);
                }

                // 在点之间稍作停顿
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            ESP_LOGI(TAG, "Replay sequence completed");
        }
    }
}

extern "C" void app_main(void)
{
    // Initialize event group
    panthera_event_group = xEventGroupCreate();
    if (panthera_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // Clear the event bits initially
    xEventGroupClearBits(panthera_event_group, ALL_EVENTS_BIT);

#if CONFIG_DEBUG_MODE
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif
#endif

    // Setup motor controller
    damiao::Motor_Control::initialize(GPIO_NUM_17, GPIO_NUM_18);
    dm = &damiao::Motor_Control::getInstance();
    M1 = new damiao::Motor(damiao::DM4310, 0x01, 0x11);
    M2 = new damiao::Motor(damiao::DM4340, 0x02, 0x12);
    M3 = new damiao::Motor(damiao::DM4340, 0x03, 0x13);
    M4 = new damiao::Motor(damiao::DM4340, 0x04, 0x14);
    M5 = new damiao::Motor(damiao::DM4310, 0x05, 0x15);
    M6 = new damiao::Motor(damiao::DM4310, 0x06, 0x16);
    M7 = new damiao::Motor(damiao::DMH3510, 0x07, 0x17);

    dm->add_motor(M1);
    dm->add_motor(M2);
    dm->add_motor(M3);
    dm->add_motor(M4);
    dm->add_motor(M5);
    dm->add_motor(M6);
    dm->add_motor(M7);

    // Setup robot kinematic model
    robotics::Link links[6];
    links[0] = robotics::Link(0.0f, 0.1005f, 0.0f, -PI / 2.0f, robotics::R, 0.0f);
    links[1] = robotics::Link(0.0f, 0.0f, 0.18f, 0.0f, robotics::R, PI);
    links[2] = robotics::Link(0.0f, 0.0f, 0.188809f, 0.0f, robotics::R, 162.429f * PI / 180.0f);
    links[3] = robotics::Link(0.0f, 0.0f, 0.08f, -PI / 2.0f, robotics::R, 17.5715f * PI / 180.0f);
    links[4] = robotics::Link(0.0f, 0.0f, 0.0f, PI / 2.0f, robotics::R, PI / 2.0f);
    links[5] = robotics::Link(0.0f, 0.184f, 0.0f, PI / 2.0f, robotics::R, -PI / 2.0f);
    panther6dof = new robotics::Serial_Link<6>(links);

#if CONFIG_DEBUG_MODE
    // Initialize argtable for panthera_enable command
    panthera_enable_args.enable = arg_strn(NULL, NULL, "true|false", 1, 1, "Enable or disable panthera");
    panthera_enable_args.end = arg_end(2);
    const esp_console_cmd_t enable_panthera = {
        .command = "panthera_enable",
        .help = "Enable or disable the panthera (usage: panthera_enable true|false)",
        .hint = NULL,
        .func = enable_panthera_cmd,
        .argtable = &panthera_enable_args,
        .func_w_context = NULL,
        .context = NULL,
    };

    // Initialize argtable for panthera_home command
    const esp_console_cmd_t home_panthera = {
        .command = "panthera_home",
        .help = "Move panthera to home position (all joints at 0)",
        .hint = NULL,
        .func = home_panthera_cmd,
        .argtable = NULL,
        .func_w_context = NULL,
        .context = NULL,
    };

    // Initialize argtable for panthera_range_test command
    const esp_console_cmd_t panthera_range_test = {
        .command = "panthera_range_test",
        .help = "Test inverse kinematics for range of points",
        .hint = NULL,
        .func = panthera_range_test_cmd,
        .argtable = NULL,
        .func_w_context = NULL,
        .context = NULL,
    };

    // Initialize argtable for ikine_test command (not used for real machine, only for testing)
    const esp_console_cmd_t ikine_test = {
        .command = "ikine_test",
        .help = "Test inverse kinematics",
        .hint = NULL,
        .func = ikine_test_cmd,
        .argtable = NULL,
        .func_w_context = NULL,
        .context = NULL,
    };

    // Initialize argtable for move_panthera command
    move_panthera_args.x = arg_dbl0("x", "x", "x", "X position in meters");
    move_panthera_args.y = arg_dbl0("y", "y", "y", "Y position in meters");
    move_panthera_args.z = arg_dbl0("z", "z", "z", "Z position in meters");
    move_panthera_args.end = arg_end(3);

    // Initialize argtable for panthera_move command
    const esp_console_cmd_t move_panthera = {
        .command = "panthera_move",
        .help = "Move panthera to specified x, y, z position",
        .hint = NULL,
        .func = move_panthera_cmd,
        .argtable = &move_panthera_args,
        .func_w_context = NULL,
        .context = NULL,
    };

    const esp_console_cmd_t zero_panthera = {
        .command = "panthera_zero",
        .help = "Zero panthera",
        .hint = NULL,
        .func = zero_panthera_cmd,
        .argtable = NULL,
        .func_w_context = NULL,
        .context = NULL,
    };

    // Initialize argtable for gripper_control command
    gripper_control_args.angle = arg_dbl0("a", "angle", "angle", "Gripper angle in radians");
    gripper_control_args.end = arg_end(1);

    const esp_console_cmd_t gripper_control = {
        .command = "gripper_control",
        .help = "Control gripper angle (in radians)",
        .hint = NULL,
        .func = gripper_control_cmd,
        .argtable = &gripper_control_args,
        .func_w_context = NULL,
        .context = NULL,
    };

    // Initialize argtable for panthera_gripper command
    panthera_gripper_args.x = arg_int0("x", "x", "x", "X pixel coordinate");
    panthera_gripper_args.y = arg_int0("y", "y", "y", "Y pixel coordinate");
    panthera_gripper_args.end = arg_end(2);

    const esp_console_cmd_t panthera_gripper = {
        .command = "panthera_gripper",
        .help = "Move panthera gripper to specified pixel coordinates (usage: panthera_gripper <x> <y>)",
        .hint = NULL,
        .func = panthera_gripper_cmd,
        .argtable = &panthera_gripper_args,
        .func_w_context = NULL,
        .context = NULL,
    };

    // Initialize argtable for panthera_replay command
    const esp_console_cmd_t panthera_replay = {
        .command = "panthera_replay",
        .help = "Replay the panthera movement",
        .hint = NULL,
        .func = panthera_replay_cmd,
        .argtable = NULL,
        .func_w_context = NULL,
        .context = NULL,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&enable_panthera));
    ESP_ERROR_CHECK(esp_console_cmd_register(&home_panthera));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_range_test));
    ESP_ERROR_CHECK(esp_console_cmd_register(&ikine_test));
    ESP_ERROR_CHECK(esp_console_cmd_register(&move_panthera));
    ESP_ERROR_CHECK(esp_console_cmd_register(&zero_panthera));
    ESP_ERROR_CHECK(esp_console_cmd_register(&gripper_control));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_gripper));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_replay));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
#endif

    xTaskCreate(panthera_cmd_task, "panthera_cmd_task", 6 * 1024, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(50));
        dm->refersh_motor_status(*M1);
        dm->refersh_motor_status(*M2);
        dm->refersh_motor_status(*M3);
        dm->refersh_motor_status(*M4);
        dm->refersh_motor_status(*M5);
        dm->refersh_motor_status(*M6);
        dm->refersh_motor_status(*M7);

        // std::cout << "M1: " << M1->motor_fb_param.position << ", M2: " << M2->motor_fb_param.position << ", M3: " << M3->motor_fb_param.position << ", M4: " << M4->motor_fb_param.position << ", M5: " << M5->motor_fb_param.position << ", M6: " << M6->motor_fb_param.position << ", M7: " << M7->motor_fb_param.position << std::endl;
    }
}
