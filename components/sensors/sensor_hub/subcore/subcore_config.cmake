# subcore_project.cmake file must be manually included in the project's top level CMakeLists.txt before project()

function(sensor_hub_add_lp_subcore component_dir)
    get_filename_component(component_dir_abs "${component_dir}" ABSOLUTE)
    get_filename_component(subcore_project_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}" ABSOLUTE)

    if(NOT COMMAND esp_amp_add_subcore_project)
        set(SENSOR_HUB_LP_COMPONENT_DIR "${component_dir_abs}" CACHE INTERNAL "sensor_hub lp component dir")
        return()
    endif()

    get_property(subcore_registered GLOBAL PROPERTY SENSOR_HUB_LP_SUBCORE_REGISTERED)
    if(subcore_registered)
        return()
    endif()

    set_property(GLOBAL PROPERTY SENSOR_HUB_LP_COMPONENT_DIR "${component_dir_abs}")
    set_property(GLOBAL PROPERTY SENSOR_HUB_LP_SUBCORE_REGISTERED TRUE)
    idf_build_set_property(EXTRA_CMAKE_ARGS "-DSENSOR_HUB_LP_COMPONENT_DIR=${component_dir_abs}" APPEND)
    esp_amp_add_subcore_project(subcore_sensor_hub_lp_detect "${subcore_project_dir}" EMBED)
endfunction()
