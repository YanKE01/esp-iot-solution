idf_component_register(SRC_DIRS "."
                    INCLUDE_DIRS "." "include"
                    REQUIRES servo console esp_wifi spiffs esp_http_client mbedtls)

set_source_files_properties(
    "arm_control.cpp" "app_wifi.cpp"
    PROPERTIES
    COMPILE_OPTIONS
        "-Wno-missing-field-initializers"
)
