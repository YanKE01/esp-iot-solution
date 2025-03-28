/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/Geometry>
#include <cmath>

namespace robotbox {

class RevoluteDH {
public:
    double m_d;
    double m_a;
    double m_alpha;
    double m_offset;
    double m_theta;

    RevoluteDH(double d, double a, double alpha, double offset = 0.0, double theta = 0.0)
        : m_d(d), m_a(a), m_alpha(alpha), m_offset(offset), m_theta(theta) {}
};

class RobotBox {
public:
    explicit RobotBox(const std::vector<RevoluteDH> &links);
    Eigen::MatrixXd homogeneous(std::array<double, 5> link);
private:
    std::vector<std::array<double, 5>> m_links; /*!< d, a, alpha, offset, theta */
};

}
