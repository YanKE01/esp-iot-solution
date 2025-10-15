idf_component_register(SRC_DIRS "." "model" "imu"
                    INCLUDE_DIRS "." "model" "imu")

idf_component_get_property(tflite_micro_lib espressif__esp-tflite-micro COMPONENT_LIB)
if(tflite_micro_lib)
    target_compile_options(${tflite_micro_lib} PRIVATE -Wno-shadow)
endif()
