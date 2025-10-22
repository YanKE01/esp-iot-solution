idf_component_register(SRCS "uac_audio_test.c"
                    INCLUDE_DIRS ".")

idf_build_get_property(build_components BUILD_COMPONENTS)
if(tinyusb IN_LIST build_components)
    set(tinyusb_name tinyusb) # Local component
else()
    set(tinyusb_name espressif__tinyusb) # Managed component
endif()

idf_component_get_property(tusb_lib ${tinyusb_name} COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tusb")
target_sources(${tusb_lib} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tusb/usb_descriptors.c")
