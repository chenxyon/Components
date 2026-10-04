#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "http_page_module.h"

typedef struct {
    char ap_ssid[32];
    char ap_password[64];
    bool auto_reconnect;
} http_server_wifi_config_t;

typedef struct {
    http_server_wifi_config_t wifi;
    const http_page_module_t *pages;
    int page_count;
} http_server_config_t;

esp_err_t http_server_init(const http_server_config_t *config);
esp_err_t http_server_connect_sta(const char *ssid, const char *password);
httpd_handle_t http_server_get_handle(void);
const char *http_server_get_ip(void);
bool http_server_wifi_is_connected(void);

#endif
