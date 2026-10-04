idf_component_register(
    SRCS "src/http_server.c" "src/page_log.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_http_server
    PRIV_REQUIRES app_log nvs_flash esp_event
)
