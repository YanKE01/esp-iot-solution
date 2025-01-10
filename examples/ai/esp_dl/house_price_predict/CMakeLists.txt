# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(TEST_DATA_CSV ${CMAKE_CURRENT_SOURCE_DIR}/main/test_data/house_price_test.csv)
set(TEST_DATA_C ${CMAKE_CURRENT_SOURCE_DIR}/main/test_data/test_data.c)
set(CSV_TO_ARRAY_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/main/test_data/csv2array.py)
set(SCALING_PARAMS_JSON ${CMAKE_CURRENT_SOURCE_DIR}/main/test_data/scaling_params.json)

add_custom_command(
    OUTPUT ${TEST_DATA_C}
    COMMAND ${CMAKE_COMMAND} -E env python3 ${CSV_TO_ARRAY_SCRIPT} --output_c ${TEST_DATA_C} --scaling_params ${SCALING_PARAMS_JSON} ${TEST_DATA_CSV}
    DEPENDS ${TEST_DATA_CSV} ${CSV_TO_ARRAY_SCRIPT}
    COMMENT "Generating ${TEST_DATA_C} from ${TEST_DATA_CSV}"
)

add_custom_target(generate_test_data ALL
    DEPENDS ${TEST_DATA_C})

project(house_price_predict)
