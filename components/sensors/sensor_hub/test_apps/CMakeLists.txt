# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(ESP_AMP_PATH /home/yanke/project/esp-amp)

set(EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/unit-test-app/components"
                         "${ESP_AMP_PATH}/components"
                         "../../sensor_hub")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(sensor_hub_test)
