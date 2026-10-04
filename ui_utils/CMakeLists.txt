idf_component_register(
    SRCS "src/ui_utils.c"
         "src/ui_manager.c"
         "src/input_device.c"
         "src/settings_ui.c"
         "src/ui_reset.c"
         "src/version_check.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl__lvgl
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ENABLE_KEYPAD_INPUT=1
    ENABLE_TOUCHPAD_INPUT=1
    ENABLE_SETTINGS_UI=1
)
