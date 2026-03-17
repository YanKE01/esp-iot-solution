idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "." "../../include"
    REQUIRES ulp
)

target_link_libraries(${COMPONENT_LIB} INTERFACE "-u lp_fake_humiture_init")
