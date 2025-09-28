/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <iostream>
#include <cmath>
#include "matrix.hpp"
#include "robotics.hpp"
#include "dmbot.hpp"

using namespace std;

extern "C" void app_main(void)
{

    // Setup motor controller
    // damiao::Motor_Control::initialize(GPIO_NUM_17, GPIO_NUM_18);
    // damiao::Motor_Control &dm = damiao::Motor_Control::getInstance();
    // damiao::Motor M1(damiao::DM4310, 0x01, 0x11);
    // damiao::Motor M2(damiao::DM4340, 0x02, 0x12);
    // damiao::Motor M3(damiao::DM4340, 0x03, 0x13);
    // damiao::Motor M4(damiao::DM4340, 0x04, 0x14);

    // dm.add_motor(&M1);
    // dm.add_motor(&M2);
    // dm.add_motor(&M3);
    // dm.add_motor(&M4);

    // dm.set_zero_pos(M1);
    // dm.set_zero_pos(M2);
    // dm.set_zero_pos(M3);
    // dm.set_zero_pos(M4);

    // dm.enable(M1);
    // dm.enable(M2);
    // dm.enable(M3);
    // dm.enable(M4);

    // Setup robot kinematic model
    robotics::Link links[6];
    links[0] = robotics::Link(0.0f, 0.1005f, 0.0f, -PI / 2.0f, robotics::R, 0.0f);
    links[1] = robotics::Link(0.0f, 0.0f, 0.18f, 0.0f, robotics::R, PI);
    links[2] = robotics::Link(0.0f, 0.0f, 0.188809f, 0.0f, robotics::R, 162.429f * PI / 180.0f);
    links[3] = robotics::Link(0.0f, 0.0f, 0.08f, -PI / 2.0f, robotics::R, 17.5715f * PI / 180.0f);
    links[4] = robotics::Link(0.0f, 0.0f, 0.0f, PI / 2.0f, robotics::R, PI / 2.0f);
    links[5] = robotics::Link(0.0f, 0.184f, 0.0f, PI / 2.0f, robotics::R, -PI / 2.0f);
    robotics::Serial_Link<6> panther6dof(links);

    vTaskDelay(pdMS_TO_TICKS(1000));
    // Test forward kinematics
    float q[6] = {0.0, DEG2RAD(60.0), DEG2RAD(-45.0), DEG2RAD(54.0), 0.0, 0.0};

    // dm.control_pos_vel(M1, q[0], 5.0f);
    // dm.control_pos_vel(M2, q[1], 5.0f);
    // dm.control_pos_vel(M3, -q[2], 5.0f);
    // dm.control_pos_vel(M4, q[3], 5.0f);

    vTaskDelay(pdMS_TO_TICKS(2000));

    Matrixf<4, 4> T = panther6dof.fkine(q);

    matrix_print::print(T, "Forward Kinematics Result T");

    for (int dx = -10; dx <= 10; dx++) {
        for (int dy = 0; dy <= 20; dy++) {
            Matrixf<4, 4> T1 = T;
            T1[0][3] = T[0][3] + dx / 100.0f;  // x offset in cm
            T1[1][3] = T[1][3] + dy / 100.0f;  // y offset in cm
            // Perform inverse kinematics
            Matrixf<6, 1> q_ikine = panther6dof.ikine(T1, Matrixf<6, 1>(q));

            // Perform forward kinematics with solved joint angles
            Matrixf<4, 4> T2 = panther6dof.fkine(q_ikine);

            // Calculate position errors
            float error_x = T1[0][3] - T2[0][3];
            float error_y = T1[1][3] - T2[1][3];

            // Print joint angles in degrees
            cout << "Joint angles: [";
            for (int j = 0; j < 6; j++) {
                cout << q_ikine[j][0] * 180.0f / PI;
                if (j < 5) {
                    cout << ", ";
                }
            }
            cout << "] deg | ";

            // Print target and actual positions with errors
            cout << "Target: x=" << T1[0][3] << " y=" << T1[1][3]
                 << " | Actual: x=" << T2[0][3] << " y=" << T2[1][3]
                 << " | Error: x=" << error_x << " y=" << error_y << endl;

            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    // // Test specific points: dx=-10,dy=10; dx=0,dy=10; dx=10,dy=10; dx=0,dy=0; dx=0,dy=0
    // int test_points[5][2] = {{10, 10}, {10, 0}, {10, -10}, {0, -10}, {0, 0}};

    // for (int i = 0; i < 5; i++) {
    //     int dx = test_points[i][0];
    //     int dy = test_points[i][1];

    //     // Create target transformation matrix with offset
    //     Matrixf<4, 4> T1 = T;
    //     T1[0][3] = T[0][3] + dx / 100.0f;  // x offset in cm
    //     T1[1][3] = T[1][3] + dy / 100.0f;  // y offset in cm

    //     // Perform inverse kinematics
    //     Matrixf<6, 1> q_ikine = panther6dof.ikine(T1, Matrixf<6, 1>(q));

    //     // Perform forward kinematics with solved joint angles
    //     Matrixf<4, 4> T2 = panther6dof.fkine(q_ikine);

    //     // Calculate position errors
    //     float error_x = T1[0][3] - T2[0][3];
    //     float error_y = T1[1][3] - T2[1][3];

    //     // Print joint angles in radians
    //     cout << "Point " << i + 1 << " (dx=" << dx / 10.0f << ", dy=" << dy / 10.0f << ") - Radians: ";
    //     for (int j = 0; j < 6; j++) {
    //         cout << q_ikine[j][0];
    //         if (j < 5) {
    //             cout << ", ";
    //         }
    //     }
    //     cout << endl;

    //     // Print joint angles in degrees
    //     cout << "Point " << i + 1 << " (dx=" << dx / 10.0f << ", dy=" << dy / 10.0f << ") - Degrees: ";
    //     for (int j = 0; j < 6; j++) {
    //         cout << q_ikine[j][0] * 180.0f / PI;
    //         if (j < 5) {
    //             cout << ", ";
    //         }
    //     }
    //     cout << " deg" << endl;

    //     // Print errors
    //     cout << "Point " << i + 1 << " - Errors: error_x=" << error_x << ", error_y=" << error_y << endl;
    //     cout << "----------------------------------------" << endl;

    //     dm.control_pos_vel(M1, q_ikine[0][0], 10.0f);
    //     dm.control_pos_vel(M2, q_ikine[1][0], 10.0f);
    //     dm.control_pos_vel(M3, -q_ikine[2][0], 10.0f);
    //     dm.control_pos_vel(M4, q_ikine[3][0], 10.0f);
    //     vTaskDelay(pdMS_TO_TICKS(2000));
    // }

    // vTaskDelay(pdMS_TO_TICKS(5000));
    // dm.disable(M1);
    // dm.disable(M2);
    // dm.disable(M3);
    // dm.disable(M4);

    // while (1) {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}
