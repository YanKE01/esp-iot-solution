/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <vector>

#define P 0.0
#define A1 12.1
#define A2 8.2
#define A3 8.2
#define A4 18.5
#define MAX_LEN (A2 + A3 + A4)
#define MAX_HIGH (A1 + A2 + A3 + A4)
#define MIN_ALPHA 90.0

// 角度与弧度转换
double deg_to_rad(double deg)
{
    return deg * M_PI / 180.0;
}
double rad_to_deg(double rad)
{
    return rad * 180.0 / M_PI;
}

// 三角函数包装器（角度输入）
double cos_deg(double deg)
{
    return cos(deg_to_rad(deg));
}
double sin_deg(double deg)
{
    return sin(deg_to_rad(deg));
}
double atan2_deg(double y, double x)
{
    return rad_to_deg(atan2(y, x));
}

// 关节角度转换（保留与Python相同的转换逻辑）
double j_degree_convert(int joint, double j_or_deg)
{
    switch (joint) {
    case 1:
        return j_or_deg;
    case 2:
    case 3:
    case 4:
        return 90.0 - j_or_deg;
    default:
        return NAN;
    }
}

// 角度有效性检查
bool valid_degree(int joint, double degree)
{
    if (isnan(degree)) {
        return false;
    }
    if (degree >= 0.0 && degree <= 180.0) {
        return true;
    }
    printf("Joint %d invalid degree %.2f\n", joint, degree);
    return false;
}

bool valid_j(int joint, double j)
{
    return valid_degree(joint, j_degree_convert(joint, j));
}

// 范围检查
bool out_of_range(double length, double height)
{
    if (height > MAX_HIGH) {
        printf("Height %.2f exceeds limit %.2f\n", height, MAX_HIGH);
        return true;
    }
    if (length > MAX_LEN) {
        printf("Length %.2f exceeds limit %.2f\n", length, MAX_LEN);
        return true;
    }
    return false;
}

// 逆运动学核心计算
bool xyz_alpha_to_j123(double x, double y, double z, double alpha,
                       double *j1, double *j2, double *j3, double *j4)
{
    double length = hypot(x, y + P);
    double height = z;
    *j1 = (length == 0) ? 0.0 : atan2_deg(y + P, x);

    if (!valid_j(1, *j1) || out_of_range(length, height)) {
        return false;
    }

    double L = length - A4 * sin_deg(alpha);
    double H = height - A4 * cos_deg(alpha) - A1;

    double numerator = L * L + H * H - A2 * A2 - A3 * A3;
    double denominator = 2 * A2 * A3;
    double cos3 = numerator / denominator;

    if (fabs(cos3) > 1.0) {
        printf("A2+A3过长\n");
        return false;
    }

    double sin3 = sqrt(1.0 - cos3 * cos3);
    *j3 = rad_to_deg(atan2(sin3, cos3));

    if (!valid_j(3, *j3)) {
        return false;
    }

    double K1 = A2 + A3 * cos_deg(*j3);
    double K2 = A3 * sin_deg(*j3);
    double w = atan2_deg(K2, K1);
    *j2 = atan2_deg(L, H) - w;

    if (!valid_j(2, *j2)) {
        return false;
    }

    *j4 = alpha - *j2 - *j3;
    return valid_j(4, *j4);
}

// 逆运动学主函数（对应Python的backward_kinematics）
bool backward_kinematics(double x, double y, double z, double alpha,
                         double *deg1, double *deg2,
                         double *deg3, double *deg4)
{
    double current_alpha = alpha;
    bool valid = false;

    while (current_alpha >= MIN_ALPHA && !valid) {
        valid = xyz_alpha_to_j123(x, y, z, current_alpha,
                                  deg1, deg2, deg3, deg4);
        if (!valid) {
            current_alpha -= 1.0;
        }
    }

    if (valid) {
        *deg1 = round(j_degree_convert(1, *deg1) * 100) / 100;
        *deg2 = round(j_degree_convert(2, *deg2) * 100) / 100;
        *deg3 = round(j_degree_convert(3, *deg3) * 100) / 100;
        *deg4 = round(j_degree_convert(4, *deg4) * 100) / 100;
    }

    return valid;
}

// 正运动学计算
bool forward_kinematics(double deg1, double deg2, double deg3, double deg4,
                        double *x, double *y, double *z)
{
    double j1 = j_degree_convert(1, deg1);
    double j2 = j_degree_convert(2, deg2);
    double j3 = j_degree_convert(3, deg3);
    double j4 = j_degree_convert(4, deg4);

    if (!valid_degree(1, deg1) || !valid_degree(2, deg2) ||
            !valid_degree(3, deg3) || !valid_degree(4, deg4)) {
        return false;
    }

    double length = A2 * sin_deg(j2) +
                    A3 * sin_deg(j2 + j3) +
                    A4 * sin_deg(j2 + j3 + j4);

    double height = A1 +
                    A2 * cos_deg(j2) +
                    A3 * cos_deg(j2 + j3) +
                    A4 * cos_deg(j2 + j3 + j4);

    *x = round(length * cos_deg(j1) * 100) / 100;
    *y = round((length * sin_deg(j1) - P) * 100) / 100;
    *z = round(height * 100) / 100;

    return (*y >= 0 && *z >= 0);
}

// 验证函数（保持与Python相同的调用关系）
std::vector<int> validate(double x, double y, double z, double alpha)
{
    double deg1, deg2, deg3, deg4;

    // 调用逆运动学
    bool valid = backward_kinematics(x, y, z, alpha,
                                     &deg1, &deg2, &deg3, &deg4);

    if (!valid) {
        printf("No valid inverse kinematics solution\n");
        return {};
    }

    // 调用正运动学验证
    double x1, y1, z1;
    valid = forward_kinematics(deg1, deg2, deg3, deg4, &x1, &y1, &z1);

    // 检查误差
    if (valid &&
            fabs(x1 - x) <= 0.5 &&
            fabs(y1 - y) <= 0.5 &&
            fabs(z1 - z) <= 0.5) {

        printf("Validation success!\n");
        printf("Angles: [%.2f, %.2f, %.2f, %.2f]\n",
               deg1, deg2, deg3, deg4);
        printf("Reconstructed: (%.2f, %.2f, %.2f)\n", x1, y1, z1);
        return {int(deg1), int(deg2), int(deg3), int(deg4)};
    } else {
        printf("Validation failed\n");
    }

    return {};
}
