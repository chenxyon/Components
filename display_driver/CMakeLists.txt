idf_component_register(
    SRCS 
        "src/display_registry.c"
        "src/lvgl_port.c"
        "src/display_utils.c"
        "src/ili9341_tft.c"
        "src/ssd1306_oled.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl fonts
    PRIV_REQUIRES esp_timer driver esp_lcd
)

message(STATUS "=================================================")
message(STATUS "Display Driver Component v2.1.0")
message(STATUS "=================================================")
message(STATUS "Supported Displays:")
message(STATUS "  TFT Screens:")
message(STATUS "    - 2.4寸 ILI9341 (横屏: 320x240)")
message(STATUS "    - 2.4寸 ILI9341 (竖屏: 240x320)")
message(STATUS "  OLED Screens:")
message(STATUS "    - 0.91寸 SSD1306 (128x32)")
message(STATUS "    - 0.96寸 SSD1306 (128x64)")
message(STATUS "Features:")
message(STATUS "  - LVGL support")
message(STATUS "  - RGB565/1-bit color formats")
message(STATUS "  - SPI/I2C communication")
message(STATUS "=================================================")