# subcore project CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

if(NOT SUBCORE_BUILD)
    message(STATUS "not subcore build, skipping sensor_hub subcore project")
    return()
endif()

if(NOT SENSOR_HUB_LP_COMPONENT_DIR)
    message(FATAL_ERROR "SENSOR_HUB_LP_COMPONENT_DIR is not set. Call sensor_hub_set_lp_component_dir(...) in the user project's CMakeLists.txt before project().")
endif()

set(EXTRA_COMPONENT_DIRS "${SENSOR_HUB_LP_COMPONENT_DIR}")

include(${ESP_AMP_PATH}/components/esp_amp/cmake/subcore_project.cmake)

set(PROJECT_VER "1.0")
project(${SUBCORE_APP_NAME})
