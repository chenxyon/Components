idf_component_register(
    SRCS
        "src/wifi_manager.c"
        "src/wifi_webserver.c"
        "src/captive_portal.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        lwip
        esp_http_server
        esp_timer
)

message(STATUS "=================================================")
message(STATUS "WiFi Manager Component v2.1")
message(STATUS "=================================================")
