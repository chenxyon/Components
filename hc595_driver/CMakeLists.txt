idf_component_register(
    SRCS
        "src/hc595_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
)

message(STATUS "=================================================")
message(STATUS "HC595 Driver Component v1.0.0")
message(STATUS "=================================================")
message(STATUS "Features:")
message(STATUS "  - 74HC595 shift register driver")
message(STATUS "  - Supports cascaded chips (1-4)")
message(STATUS "  - API: hc595_init, hc595_write, hc595_clear")
message(STATUS "  - API: hc595_set_single, hc595_get_output")
message(STATUS "=================================================")
