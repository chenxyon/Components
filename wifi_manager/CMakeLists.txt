idf_component_register(
    SRCS 
        "src/wifi_manager.c"
        "src/wifi_config.c"
        "src/wifi_scan.c"
        "src/wifi_time_sync.c"
        "src/wifi_config_mode.c"
        "src/wifi_softap.c"
        "src/wifi_webserver.c"
        "src/wifi_screen_ui.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_wifi 
        esp_event 
        nvs_flash 
        lwip 
        esp_http_server
)

message(STATUS "=================================================")
message(STATUS "WIFI Manager Component")
message(STATUS "=================================================")
message(STATUS "Version: 1.0.0")
message(STATUS "=================================================")
