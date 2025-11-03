/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "panthera_ctrl.h"
#include "ui/ui.h"
#include "ui/screens/ui_RobotCtrlScreen.h"
#include "esp_log.h"

LV_IMG_DECLARE(img_panthera_ctrl);

PantheraCtrl::PantheraCtrl(damiao::Motor_Control* motor_control)
    : ESP_Brookesia_PhoneApp(
          []()
{
    ESP_Brookesia_CoreAppData_t core_data = {
        .name = "Panthera Ctrl",
        .launcher_icon = ESP_BROOKESIA_STYLE_IMAGE(&img_panthera_ctrl),
        .screen_size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100),
        .flags = {
            .enable_default_screen = 0,
            .enable_recycle_resource = 1,
            .enable_resize_visual_area = 1,
        },
    };
    return core_data;
}(),
[]()
{
    ESP_Brookesia_PhoneAppData_t phone_data = {
        .app_launcher_page_index = 0,
        .status_icon_area_index = 0,
        .status_icon_data = {
            .size = {},
            .icon = {
                .image_num = 1,
                .images = {
                    ESP_BROOKESIA_STYLE_IMAGE(&img_panthera_ctrl),
                },
            },
        },
        .status_bar_visual_mode = ESP_BROOKESIA_STATUS_BAR_VISUAL_MODE_HIDE,
        .navigation_bar_visual_mode = ESP_BROOKESIA_NAVIGATION_BAR_VISUAL_MODE_HIDE,
        .flags = {
            .enable_status_icon_common_size = 1,
            .enable_navigation_gesture = 1,
        },
    };
    return phone_data;
}()
  ),
angle_update_timer_(nullptr), motor_control_(motor_control)
{
}

PantheraCtrl::~PantheraCtrl()
{
}

bool PantheraCtrl::init(void)
{
    ESP_LOGI("panthera_ctrl", "Init");
    return true;
}

bool PantheraCtrl::run(void)
{
    ESP_LOGI("panthera_ctrl", "Run");
    ui_panthera_ctrl_init();
    lv_obj_add_event_cb(ui_EnableSwitch, ui_EnableSwitch_event_handler, LV_EVENT_VALUE_CHANGED, this);

    // Add decrease callbacks for left images
    lv_obj_add_event_cb(ui_J1LeftImage, decrease_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_J2LeftImage, decrease_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_J3LeftImage, decrease_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_J4LeftImage, decrease_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_J5LeftImage, decrease_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_J6LeftImage, decrease_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_G1LeftImage, decrease_event_handler, LV_EVENT_CLICKED, this);

    // Add increase callbacks for right images
    lv_obj_add_event_cb(ui_J1RightImage, increase_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_J2RightImage, increase_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_J3RightImage, increase_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_J4RightImage, increase_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_J5RightImage, increase_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_J6RightImage, increase_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_G1RightImage, increase_event_handler, LV_EVENT_CLICKED, this);

    // Create timer to update joint angles (100ms interval)
    if (angle_update_timer_ == nullptr) {
        angle_update_timer_ = lv_timer_create(angle_update_timer_cb, 100, this);
        if (angle_update_timer_ == nullptr) {
            ESP_LOGE("panthera_ctrl", "Failed to create angle update timer");
            return false;
        }
    }
    return true;
}

bool PantheraCtrl::pause(void)
{
    ESP_LOGI("panthera_ctrl", "Pause");
    if (angle_update_timer_ != nullptr) {
        lv_timer_pause(angle_update_timer_);
    }
    return true;
}

bool PantheraCtrl::resume(void)
{
    ESP_LOGI("panthera_ctrl", "Resume");
    if (angle_update_timer_ != nullptr) {
        lv_timer_resume(angle_update_timer_);
    }
    return true;
}

bool PantheraCtrl::back(void)
{
    ESP_LOGI("panthera_ctrl", "Back");
    notifyCoreClosed();
    return true;
}

bool PantheraCtrl::close(void)
{
    ESP_LOGI("panthera_ctrl", "Close");
    if (angle_update_timer_ != nullptr) {
        lv_timer_del(angle_update_timer_);
        angle_update_timer_ = nullptr;
    }
    return true;
}

void PantheraCtrl::ui_EnableSwitch_event_handler(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(e));
    PantheraCtrl *app = static_cast<PantheraCtrl *>(lv_event_get_user_data(e));

    if (event_code == LV_EVENT_VALUE_CHANGED) {
        bool switch_state = lv_obj_has_state(obj, LV_STATE_CHECKED);

        if (app && app->motor_control_) {
            if (switch_state) {
                ESP_LOGI("panthera_ctrl", "Enable switch turned ON - enabling all motors");
                app->motor_control_->enable_all_motors();
            } else {
                ESP_LOGI("panthera_ctrl", "Enable switch turned OFF - disabling all motors");
                app->motor_control_->disable_all_motors();
            }
        }
    }
}

void PantheraCtrl::angle_update_timer_cb(lv_timer_t *timer)
{
    PantheraCtrl *app = static_cast<PantheraCtrl *>(lv_timer_get_user_data(timer));
    if (app != nullptr) {
        app->update_joint_angles();
    }
}

void PantheraCtrl::decrease_event_handler(lv_event_t *e)
{
    PantheraCtrl *app = static_cast<PantheraCtrl *>(lv_event_get_user_data(e));
    lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(e));

    if (app == nullptr) {
        return;
    }

    // Map UI objects to master_id
    struct {
        lv_obj_t* obj;
        uint32_t master_id;
        const char* name;
    } joint_map[] = {
        {ui_J1LeftImage, 0x11, "J1"},
        {ui_J2LeftImage, 0x12, "J2"},
        {ui_J3LeftImage, 0x13, "J3"},
        {ui_J4LeftImage, 0x14, "J4"},
        {ui_J5LeftImage, 0x15, "J5"},
        {ui_J6LeftImage, 0x16, "J6"},
        {ui_G1LeftImage, 0x17, "G1"},
    };

    for (size_t i = 0; i < sizeof(joint_map) / sizeof(joint_map[0]); i++) {
        if (obj == joint_map[i].obj) {
            ESP_LOGI("panthera_ctrl", "%s decrease clicked", joint_map[i].name);
            app->handle_joint_control(joint_map[i].master_id, false);
            return;
        }
    }
}

void PantheraCtrl::increase_event_handler(lv_event_t *e)
{
    PantheraCtrl *app = static_cast<PantheraCtrl *>(lv_event_get_user_data(e));
    lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(e));

    if (app == nullptr) {
        return;
    }

    // Map UI objects to master_id
    struct {
        lv_obj_t* obj;
        uint32_t master_id;
        const char* name;
    } joint_map[] = {
        {ui_J1RightImage, 0x11, "J1"},
        {ui_J2RightImage, 0x12, "J2"},
        {ui_J3RightImage, 0x13, "J3"},
        {ui_J4RightImage, 0x14, "J4"},
        {ui_J5RightImage, 0x15, "J5"},
        {ui_J6RightImage, 0x16, "J6"},
        {ui_G1RightImage, 0x17, "G1"},
    };

    for (size_t i = 0; i < sizeof(joint_map) / sizeof(joint_map[0]); i++) {
        if (obj == joint_map[i].obj) {
            ESP_LOGI("panthera_ctrl", "%s increase clicked", joint_map[i].name);
            app->handle_joint_control(joint_map[i].master_id, true);
            return;
        }
    }
}

void PantheraCtrl::handle_joint_control(uint32_t master_id, bool increase)
{
    if (motor_control_ == nullptr) {
        return;
    }

    // Get motor by master_id
    damiao::Motor* motor = motor_control_->get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGW("panthera_ctrl", "Motor with master_id 0x%02X not found", master_id);
        return;
    }

    // Get current position (in radians) - no need to refresh, timer already updates it
    float current_position_rad = motor->motor_fb_param.position;

    // Convert to degrees
    float current_position_deg = current_position_rad * 180.0f / 3.14159265f;

    // Calculate new position (increase or decrease by 5 degrees)
    float delta_deg = increase ? 5.0f : -5.0f;
    float new_position_deg = current_position_deg + delta_deg;

    // Convert back to radians
    float new_position_rad = new_position_deg * 3.14159265f / 180.0f;

    // Send position command to motor (velocity = 0 for step movement)
    esp_err_t ret = motor_control_->pos_vel_control(*motor, new_position_rad, 5.0f);
    if (ret != ESP_OK) {
        ESP_LOGE("panthera_ctrl", "Failed to control motor with master_id 0x%02X", master_id);
    } else {
        ESP_LOGI("panthera_ctrl", "Motor 0x%02X: %.1f° -> %.1f° (%s)",
                 master_id, current_position_deg, new_position_deg,
                 increase ? "increase" : "decrease");
    }
}

void PantheraCtrl::update_joint_angles()
{
    if (motor_control_ == nullptr) {
        return;
    }

    // Get motor with master_id = 0x16
    damiao::Motor* motor = motor_control_->get_motor_by_master_id(0x16);
    if (motor == nullptr) {
        // Motor not found, show "---"
        if (ui_J6AngleLabel != nullptr) {
            lv_label_set_text(ui_J6AngleLabel, "---");
        }
        return;
    }

    // Refresh motor status to get latest position
    motor_control_->refresh_motor_status(*motor);

    // Get position and update UI label
    if (ui_J6AngleLabel != nullptr) {
        // Convert position from radians to degrees
        float angle_deg = motor->motor_fb_param.position * 180.0f / 3.14159265f;
        char angle_str[16];
        snprintf(angle_str, sizeof(angle_str), "%.1f", angle_deg);
        lv_label_set_text(ui_J6AngleLabel, angle_str);
    }
}
