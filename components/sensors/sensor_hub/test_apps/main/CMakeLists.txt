include(../../../sensor_hub/subcore/subcore_config.cmake)

idf_component_register(SRC_DIRS "."
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES unity test_utils sensor_hub esp_amp)

sensor_hub_add_lp_subcore("${CMAKE_CURRENT_LIST_DIR}/../lp")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u _binary_subcore_sensor_hub_lp_detect_bin_start")

if(CONFIG_SENSOR_INCLUDED_HUMITURE)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u virtual_humiture_init")
endif()

if(CONFIG_SENSOR_INCLUDED_IMU)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u virtual_imu_init")
endif()

if(CONFIG_SENSOR_INCLUDED_LIGHT)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u virtual_light_init")
endif()
