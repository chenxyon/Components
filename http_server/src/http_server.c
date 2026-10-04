#include "http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "HTTP_SERVER";

static httpd_handle_t g_server = NULL;
static char g_ip[16] = "0.0.0.0";
static http_server_config_t g_config;
static bool g_http_started = false;

/* ==================== 内置状态页面 ==================== */

static esp_err_t status_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    char json[128];
    snprintf(json, sizeof(json),
        "{\"wifi\":\"%s\",\"ip\":\"%s\"}",
        g_http_started ? "connected" : "disconnected",
        g_ip);
    return httpd_resp_send(req, json, strlen(json));
}

static const http_page_route_t status_routes[] = {
    { "/status", HTTP_GET, status_handler, NULL },
};

static http_page_module_t status_module = {
    .name = "status_page",
    .routes = status_routes,
    .route_count = 1,
};

/* ==================== 自定义错误页面 ==================== */

static esp_err_t custom_404_handler(httpd_req_t *req, httpd_err_code_t error) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_status(req, "404 Not Found");
    const char *html =
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<title>404</title>"
        "<style>body{font-family:Arial;text-align:center;padding:80px 20px;}"
        "h1{font-size:72px;color:#e74c3c;margin:0;}"
        "p{color:#666;font-size:18px;}</style></head><body>"
        "<h1>404</h1><p>Page not found</p>"
        "<p><a href='/'>Back to Home</a></p>"
        "</body></html>";
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

static esp_err_t custom_500_handler(httpd_req_t *req, httpd_err_code_t error) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_status(req, "500 Internal Server Error");
    const char *html =
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<title>500</title>"
        "<style>body{font-family:Arial;text-align:center;padding:80px 20px;}"
        "h1{font-size:72px;color:#e74c3c;margin:0;}"
        "p{color:#666;font-size:18px;}</style></head><body>"
        "<h1>500</h1><p>Internal Server Error</p>"
        "<p><a href='/'>Back to Home</a></p>"
        "</body></html>";
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

/* ==================== HTTP服务器启动 ==================== */

static void start_http_server(void) {
    if (g_http_started) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 8192;
    config.max_uri_handlers = 16;

    if (httpd_start(&g_server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    for (int i = 0; i < status_module.route_count; i++) {
        httpd_register_uri_handler(g_server, &status_module.routes[i]);
    }

    extern http_page_module_t page_log_get_module(void);
    http_page_module_t log_mod = page_log_get_module();
    ESP_LOGI(TAG, "Registering built-in page: %s (%d routes)", log_mod.name, log_mod.route_count);
    for (int r = 0; r < log_mod.route_count; r++) {
        httpd_register_uri_handler(g_server, &log_mod.routes[r]);
    }

    for (int p = 0; p < g_config.page_count; p++) {
        const http_page_module_t *mod = &g_config.pages[p];
        ESP_LOGI(TAG, "Registering page module: %s (%d routes)", mod->name, mod->route_count);
        for (int r = 0; r < mod->route_count; r++) {
            httpd_register_uri_handler(g_server, &mod->routes[r]);
        }
    }

    httpd_register_err_handler(g_server, HTTPD_404_NOT_FOUND, custom_404_handler);
    httpd_register_err_handler(g_server, HTTPD_500_INTERNAL_SERVER_ERROR, custom_500_handler);

    g_http_started = true;
    ESP_LOGI(TAG, "HTTP server started on port 0.0.0.0:80, IP: %s", g_ip);
}

/* ==================== WiFi事件处理 ==================== */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START: {
                ESP_LOGI(TAG, "WiFi AP started event");
                esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
                if (ap_netif != NULL) {
                    esp_netif_ip_info_t ip_info;
                    if (esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
                        snprintf(g_ip, sizeof(g_ip), IPSTR, IP2STR(&ip_info.ip));
                        ESP_LOGI(TAG, "AP IP: %s", g_ip);
                    }
                }
                start_http_server();
                break;
            }
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                ESP_LOGI(TAG, "Station connected, AID=%d", event->aid);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                ESP_LOGI(TAG, "Station disconnected, AID=%d", event->aid);
                break;
            }
            case WIFI_EVENT_STA_CONNECTED: {
                ESP_LOGI(TAG, "STA connected to AP");
                break;
            }
            case WIFI_EVENT_STA_DISCONNECTED: {
                ESP_LOGW(TAG, "STA disconnected from AP");
                break;
            }
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(g_ip, sizeof(g_ip), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "STA got IP: %s", g_ip);

        /* STA 连上后关掉 AP，AP / STA 二选一 */
        wifi_mode_t cur_mode = WIFI_MODE_NULL;
        if (esp_wifi_get_mode(&cur_mode) == ESP_OK && cur_mode == WIFI_MODE_APSTA) {
            esp_err_t mret = esp_wifi_set_mode(WIFI_MODE_STA);
            if (mret == ESP_OK) {
                ESP_LOGI(TAG, "STA connected -> switch to STA-only (AP stopped)");
            } else {
                ESP_LOGW(TAG, "Failed to switch to STA-only: %s", esp_err_to_name(mret));
            }
        }

        start_http_server();
    }
}

/* ==================== 启动AP模式 ==================== */

static void start_ap_mode(const http_server_config_t *config) {
    ESP_LOGI(TAG, "Starting AP mode: %s", config->wifi.ap_ssid);

    esp_wifi_set_mode(WIFI_MODE_APSTA);

    wifi_config_t ap_config = {0};
    strncpy((char *)ap_config.ap.ssid, config->wifi.ap_ssid, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen(config->wifi.ap_ssid);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (strlen(config->wifi.ap_password) > 0) {
        strncpy((char *)ap_config.ap.password, config->wifi.ap_password, sizeof(ap_config.ap.password) - 1);
    } else {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* ==================== STA连接任务 ==================== */

#define STA_MAX_RETRY 10
static bool s_sta_connecting = false;

static void sta_connect_task(void *arg) {
    char *ssid = ((char *)arg);
    char *password = ssid + 33;

    wifi_config_t sta_config = {0};
    strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid) - 1);
    strncpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);

    for (int retry = 0; retry < STA_MAX_RETRY; retry++) {
        ESP_LOGI(TAG, "STA connecting to: %s (attempt %d/%d)", ssid, retry + 1, STA_MAX_RETRY);
        esp_err_t err = esp_wifi_connect();
        if (err == ESP_OK) {
            /* 等待连接结果 */
            vTaskDelay(pdMS_TO_TICKS(5000));
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                ESP_LOGI(TAG, "STA connected to: %s", ssid);
                free(ssid);
                vTaskDelete(NULL);
                return;
            }
        }
        ESP_LOGW(TAG, "STA connect attempt %d failed", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    /* 10次都失败，切回AP模式 */
    ESP_LOGW(TAG, "STA connect failed after %d attempts, switching to AP mode", STA_MAX_RETRY);
    esp_wifi_stop();
    start_ap_mode(&g_config);
    free(ssid);
    vTaskDelete(NULL);
}

/* ==================== 公开接口 ==================== */

esp_err_t http_server_init(const http_server_config_t *config) {
    if (config == NULL) return ESP_ERR_INVALID_ARG;

    memcpy(&g_config, config, sizeof(http_server_config_t));
    ESP_LOGI(TAG, "Initializing WiFi...");

    if (strlen(config->wifi.ap_ssid) == 0) {
        ESP_LOGE(TAG, "AP SSID is empty");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    /* 读取NVS检查是否有已保存的WiFi配置 */
    nvs_handle_t nvs;
    bool has_saved_wifi = false;
    char saved_ssid[33] = {0};
    char saved_password[65] = {0};

    if (nvs_open("wifi_config", NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t saved = 0;
        nvs_get_u8(nvs, "saved", &saved);
        if (saved) {
            size_t len = sizeof(saved_ssid);
            nvs_get_str(nvs, "ssid", saved_ssid, &len);
            len = sizeof(saved_password);
            nvs_get_str(nvs, "password", saved_password, &len);
            has_saved_wifi = strlen(saved_ssid) > 0;
        }
        nvs_close(nvs);
    }

    if (has_saved_wifi) {
        /* 有保存的WiFi，先开AP（确保esp_wifi_start被调用），再尝试STA连接 */
        ESP_LOGI(TAG, "Found saved WiFi: %s, starting AP + STA connect...", saved_ssid);
        start_ap_mode(config);
        char *params = malloc(33 + 65);
        if (params) {
            memset(params, 0, 33 + 65);
            strncpy(params, saved_ssid, 32);
            strncpy(params + 33, saved_password, 64);
            xTaskCreatePinnedToCore(sta_connect_task, "sta_connect", 4096, params, 5, NULL, 0);
        }
    } else {
        /* 没有保存的WiFi，直接进AP热点 */
        ESP_LOGI(TAG, "No saved WiFi, starting AP mode");
        start_ap_mode(config);
    }

    return ESP_OK;
}

esp_err_t http_server_connect_sta(const char *ssid, const char *password) {
    if (ssid == NULL || strlen(ssid) == 0) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    char *params = malloc(33 + 65);
    if (!params) return ESP_ERR_NO_MEM;

    memset(params, 0, 33 + 65);
    strncpy(params, ssid, 32);
    if (password) strncpy(params + 33, password, 64);
    xTaskCreatePinnedToCore(sta_connect_task, "sta_connect", 4096, params, 5, NULL, 0);
    return ESP_OK;
}

httpd_handle_t http_server_get_handle(void) {
    return g_server;
}

const char *http_server_get_ip(void) {
    return g_ip;
}

bool http_server_wifi_is_connected(void) {
    return g_http_started;
}
