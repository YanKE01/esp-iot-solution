idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_uart
)

set_source_files_properties(
    "servo.cpp"
    PROPERTIES
    COMPILE_OPTIONS
        "-Wno-missing-field-initializers"
)
