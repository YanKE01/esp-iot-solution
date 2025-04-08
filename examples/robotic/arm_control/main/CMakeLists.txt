idf_component_register(SRC_DIRS "." "ui/"
                    INCLUDE_DIRS "." "include" "ui/"
                    REQUIRES dofbot console esp_wifi spiffs esp_http_client mbedtls esp_driver_jpeg nvs_flash json)

set_source_files_properties(
    "arm_control.cpp" "app_wifi.cpp" "app_manager.cpp"
    PROPERTIES
    COMPILE_OPTIONS
        "-Wno-missing-field-initializers"
)
