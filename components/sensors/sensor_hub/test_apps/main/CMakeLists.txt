idf_component_register(SRC_DIRS "."
                       PRIV_INCLUDE_DIRS "."
                       PRIV_REQUIRES unity test_utils sensor_hub)

if(CONFIG_SENSOR_INCLUDED_HUMITURE)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u virtual_sht3x_detect")
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u virtual_hst221_detect")
endif()
