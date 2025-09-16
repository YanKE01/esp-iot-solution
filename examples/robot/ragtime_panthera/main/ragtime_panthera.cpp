/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <iostream>
#include "matrix.hpp"
#include "robotics.hpp"

using namespace std;

extern "C" void app_main(void)
{
    robotics::Link links[6];
    links[0] = robotics::Link(0.0f, 0.1005f, 0.0f, -PI / 2.0f, robotics::R, 0.0f);
    links[1] = robotics::Link(0.0f, 0.0f, 0.18f, 0.0f, robotics::R, PI);
    links[2] = robotics::Link(0.0f, 0.0f, 0.188809f, 0.0f, robotics::R, 162.429f * PI / 180.0f);
    links[3] = robotics::Link(0.0f, 0.0f, 0.08f, -PI / 2.0f, robotics::R, 17.5715f * PI / 180.0f);
    links[4] = robotics::Link(0.0f, 0.0f, 0.0f, PI / 2.0f, robotics::R, PI / 2.0f);
    links[5] = robotics::Link(0.0f, 0.184f, 0.0f, PI / 2.0f, robotics::R, -PI / 2.0f);
    robotics::Serial_Link<6> panther6dof(links);

    float q[6] = {0.0, DEG2RAD(60.0), DEG2RAD(-71.0), DEG2RAD(71.0), 0.0, 0.0};

    Matrixf<4, 4> T = panther6dof.fkine(q);

    matrix_print::print(T, "Forward Kinematics Result T");

    Matrixf<4, 4> T1 = T;
    T1[0][3] = T[0][3] + 0.1;
    matrix_print::print(T1, "Forward Kinematics Result T1");

    Matrixf<6, 1> q_ikine = panther6dof.ikine(T1, Matrixf<6, 1>(q));
    for (int i = 0; i < 6; i++) {
        cout << (i + 1) << ": " << RAD2DEG(q_ikine[i][0]) << " deg" << endl;
    }
}
