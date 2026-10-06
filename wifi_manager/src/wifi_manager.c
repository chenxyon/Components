/**
 * @file     wifi_manager.c
 * @brief    WiFi 配网组件 —— 统一管理 STA 连接与 AP 配网
 *
 * 工作模式：
 *   AUTO : 先尝试 STA 连接 → 失败 N 次后自动进 AP+Web 配网
 *   CONNECT: 只尝试 STA 连接（永不进配网）
 *   CONFIG : 只启动 AP+Web 配网，不连 STA
 *
 * 事件回调在 wifi_prov 中实现，现已合并到此文件
 */
#include "wifi_manager.h"
#include "wifi_webserver_internal.h"

#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string.h>

static const char *TAG = "wifi_mgr";

/* ==================== 内部状态 ==================== */
static bool             s_ready       = false;
static wifi_manager_mode_t s_mode     = WIFI_MGR_MODE_AUTO;

static uint8_t          s_retry_count = 0;
static bool             s_ap_started  = false;
static bool             s_scan_running = false;
static bool             s_scan_done    = false;
static uint32_t         s_scan_start_ms = 0;
static wifi_ap_record_t s_scan_buf[20];
static uint16_t         s_scan_count   = 0;
#define SCAN_TIMEOUT_MS 15000

/* 回调 */
static wifi_manager_event_cb_t s_event_cb    = NULL;
static void                   *s_event_cb_ctx = NULL;

/* 配置（拷贝自入参） */
static wifi_manager_config_t s_cfg;

/* ==================== 事件派发 ==================== */
static void emit_event(wifi_event_t ev)
{
    if (s_event_cb != NULL) {
        s_event_cb(ev, s_event_cb_ctx);
    }
}

/* ==================== WiFi 事件处理 ==================== */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base != WIFI_EVENT) return;

    switch (id) {
    case WIFI_EVENT_STA_START:
        emit_event(WIFI_EV_CONNECTING);
        break;

    case WIFI_EVENT_STA_CONNECTED:
        emit_event(WIFI_EV_CONNECTED);
        ESP_LOGI(TAG, "已关联热点，等待获取 IP …");
        break;

    case WIFI_EVENT_STA_DISCONNECTED:
        emit_event(WIFI_EV_DISCONNECTED);
        if (s_mode == WIFI_MGR_MODE_AUTO && !s_ap_started) {
            s_retry_count++;
            ESP_LOGW(TAG, "断开，重试 %u/%u",
                     s_retry_count, s_cfg.max_retry);
            if (s_retry_count >= s_cfg.max_retry && s_cfg.max_retry != 0) {
                ESP_LOGW(TAG, "重试耗尽，进入配网模式");
                emit_event(WIFI_EV_FALLBACK);
                s_ap_started = true;
                wifi_config_mode_start_internal(2);  /* WIFI_CONFIG_MODE_WEB */
            } else if (s_cfg.auto_reconnect) {
                vTaskDelay(pdMS_TO_TICKS(s_cfg.retry_interval_ms));
                esp_wifi_connect();
            }
        } else if (s_cfg.auto_reconnect) {
            esp_wifi_connect();
        }
        break;

    case WIFI_EVENT_AP_START:
        s_ap_started = true;
        emit_event(WIFI_EV_AP_STARTED);
        break;

    case WIFI_EVENT_SCAN_DONE:
    {
        s_scan_running = false;
        s_scan_done    = true;
        wifi_event_sta_scan_done_t *evt =
            (wifi_event_sta_scan_done_t *)data;
        if (evt->status == ESP_OK) {
            uint16_t n = sizeof(s_scan_buf) / sizeof(s_scan_buf[0]);
            esp_wifi_scan_get_ap_records(&n, s_scan_buf);
            s_scan_count = n;
            ESP_LOGI(TAG, "扫描完成，发现 %u 个 AP", s_scan_count);
        }
        break;
    }

    default:
        break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t base,
                             int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base != IP_EVENT || id != IP_EVENT_STA_GOT_IP) return;

    s_retry_count = 0;
    emit_event(WIFI_EV_GOT_IP);

    /* STA 连上后切回 STA-only，关 AP */
    wifi_mode_t m = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&m) == ESP_OK && m == WIFI_MODE_APSTA) {
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_stop();
        esp_wifi_start();
        s_ap_started = false;
    }

    /* NTP */
    if (s_cfg.enable_ntp && s_cfg.ntp_server != NULL) {
        sntp_setservername(0, (char *)s_cfg.ntp_server);
        sntp_init();
    }
}

/* ==================== 公共 API ==================== */

esp_err_t wifi_manager_init(const wifi_manager_config_t *config)
{
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    if (s_ready)        return ESP_OK;  /* 已初始化，幂等 */

    memcpy(&s_cfg, config, sizeof(s_cfg));
    s_mode = config->mode;

    /* NVS */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 异常，擦除重建");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(err);

    /* 网络栈 */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    /* WiFi 驱动 */
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* 注册事件 */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               ip_event_handler, NULL));

    s_ready = true;
    ESP_LOGI(TAG, "WiFi 管理器初始化完成，mode=%d", (int)s_mode);

    /* 按模式启动 */
    switch (s_mode) {
    case WIFI_MGR_MODE_AUTO:
    case WIFI_MGR_MODE_CONNECT:
        /* 先尝试 STA 连接 */
        {
            char ssid[33] = {0}, pw[65] = {0};
            if (wifi_manager_load_config(ssid, pw, sizeof(ssid), sizeof(pw)) == ESP_OK
                && ssid[0] != '\0') {
                ESP_LOGI(TAG, "找到已保存配置 %s，尝试连接", ssid);
                wifi_manager_sta_connect();
            } else {
                ESP_LOGI(TAG, "无保存配置，进入配网模式");
                wifi_manager_start_config_portal();
            }
        }
        break;

    case WIFI_MGR_MODE_CONFIG:
        wifi_manager_start_config_portal();
        break;
    }

    return ESP_OK;
}

esp_err_t wifi_manager_deinit(void)
{
    if (!s_ready) return ESP_OK;
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler);
    esp_wifi_stop();
    esp_wifi_deinit();
    s_ready = false;
    s_ap_started = false;
    return ESP_OK;
}

void wifi_manager_set_event_cb(wifi_manager_event_cb_t cb, void *ctx)
{
    s_event_cb    = cb;
    s_event_cb_ctx = ctx;
}

/* ==================== 状态查询 ==================== */

esp_err_t wifi_manager_get_status(wifi_status_t *st)
{
    if (!st) return ESP_ERR_INVALID_ARG;
    memset(st, 0, sizeof(*st));

    wifi_ap_record_t rec;
    if (esp_wifi_sta_get_ap_info(&rec) != ESP_OK) {
        st->connected = false;
        strcpy(st->ip_address, "0.0.0.0");
        return ESP_OK;
    }
    st->connected        = true;
    strncpy(st->ssid, (char *)rec.ssid, sizeof(st->ssid) - 1);
    st->rssi             = rec.rssi;
    st->signal_level     = wifi_manager_rssi_to_level(rec.rssi);
    st->ip_address[0]    = '\0';
    esp_netif_ip_info_t ip;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
        snprintf(st->ip_address, sizeof(st->ip_address),
                 IPSTR, IP2STR(&ip.ip));
    }
    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    return esp_wifi_sta_get_ap_info(NULL) == ESP_OK;
}

wifi_signal_level_t wifi_manager_rssi_to_level(int8_t rssi)
{
    if      (rssi >= -50) return WIFI_SIGNAL_STRONG;
    else if (rssi >= -60) return WIFI_SIGNAL_MEDIUM;
    else if (rssi >= -70) return WIFI_SIGNAL_WEAK;
    else if (rssi >= -80) return WIFI_SIGNAL_VERY_WEAK;
    else                  return WIFI_SIGNAL_NONE;
}

/* ==================== STA 连接管理 ==================== */

esp_err_t wifi_manager_sta_connect(void)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    s_retry_count = 0;
    s_ap_started  = false;

    /* 如果当前是 AP 模式，切回 APSTA */
    wifi_mode_t m = WIFI_MODE_NULL;
    esp_wifi_get_mode(&m);
    if (m == WIFI_MODE_AP) {
        esp_wifi_set_mode(WIFI_MODE_APSTA);
        esp_wifi_start();
    }

    /* 加载保存的密码 */
    char ssid[33] = {0}, pw[65] = {0};
    if (wifi_manager_load_config(ssid, pw, sizeof(ssid), sizeof(pw)) != ESP_OK
        || ssid[0] == '\0') {
        ESP_LOGW(TAG, "无保存配置，无法连接 STA");
        return ESP_ERR_NOT_FOUND;
    }

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, pw, sizeof(wc.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_connect());
    ESP_LOGI(TAG, "开始连接 STA: %s", ssid);
    return ESP_OK;
}

esp_err_t wifi_manager_sta_disconnect(void)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    esp_wifi_disconnect();
    return ESP_OK;
}

/* ==================== AP / 配网管理 ==================== */

esp_err_t wifi_manager_start_config_portal(void)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    /* 停止 Web 和 STA，再开 AP */
    wifi_webserver_stop_internal();
    esp_wifi_disconnect();

    /* 设回 APSTA 让扫描可用 */
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_start();

    /* 配置 SoftAP */
    const char *ssid  = s_cfg.fallback_ssid  ? s_cfg.fallback_ssid  : "XIAOLE_WIFI";
    const char *pw    = s_cfg.fallback_pw    ? s_cfg.fallback_pw    : "12345678";
    uint8_t     ch    = s_cfg.fallback_channel ? s_cfg.fallback_channel : 6;

    wifi_config_t ap_cfg = {0};
    strncpy((char *)ap_cfg.ap.ssid, ssid, sizeof(ap_cfg.ap.ssid) - 1);
    strncpy((char *)ap_cfg.ap.password, pw, sizeof(ap_cfg.ap.password) - 1);
    ap_cfg.ap.channel      = ch;
    ap_cfg.ap.ssid_hidden  = false;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode     = strlen(pw) > 0 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* 清旧配置，强制下次走配网页 */
    wifi_manager_clear_config();
    s_ap_started = true;

    wifi_webserver_start_internal();
    ESP_LOGI(TAG, "配网模式已启动：SSLA=%s 通道=%d", ssid, ch);
    return ESP_OK;
}

esp_err_t wifi_manager_stop_config_portal(void)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    wifi_webserver_stop_internal();
    s_ap_started = false;
    /* 切回 APSTA 保持扫描能力 */
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_start();
    return ESP_OK;
}

wifi_manager_mode_t wifi_manager_get_mode(void)
{
    return s_mode;
}

/* ==================== 扫描 ==================== */

esp_err_t wifi_manager_scan_start(void)
{
    if (!s_ready || s_scan_running) return ESP_ERR_INVALID_STATE;
    s_scan_running = true;
    s_scan_done    = false;
    s_scan_start_ms = 0;
    return esp_wifi_scan_start(NULL, false);
}

bool wifi_manager_scan_is_running(void)
{
    return s_scan_running;
}

esp_err_t wifi_manager_scan_get_results(wifi_ap_record_t *buf, uint16_t *max_count)
{
    if (!buf || !max_count) return ESP_ERR_INVALID_ARG;
    if (s_scan_running) {
        /* 超时兜底 */
        if (s_scan_start_ms != 0) {
            uint32_t elapsed = esp_timer_get_time() / 1000 - s_scan_start_ms;
            if (elapsed >= SCAN_TIMEOUT_MS) {
                s_scan_running = false;
                s_scan_done    = true;
                s_scan_count   = 0;
                ESP_LOGW(TAG, "扫描超时");
            }
        }
        return ESP_ERR_NOT_FINISHED;
    }
    if (!s_scan_done) return ESP_ERR_NOT_FOUND;
    uint16_t n = s_scan_count < *max_count ? s_scan_count : *max_count;
    memcpy(buf, s_scan_buf, n * sizeof(wifi_ap_record_t));
    *max_count = n;
    return ESP_OK;
}

/* ==================== NVS 配置 ==================== */

esp_err_t wifi_manager_save_config(const char *ssid, const char *password)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &h));
    ESP_ERROR_CHECK(nvs_set_str(h, "ssid", ssid));
    if (password) ESP_ERROR_CHECK(nvs_set_str(h, "pw", password));
    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t wifi_manager_load_config(char *ssid, char *password,
                                    size_t ssid_len, size_t pw_len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("wifi", NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t sl = ssid_len;
    size_t pl = pw_len;
    nvs_get_str(h, "ssid", ssid, &sl);
    nvs_get_str(h, "pw", password, &pl);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t wifi_manager_clear_config(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_erase_key(h, "ssid");
    nvs_erase_key(h, "pw");
    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}
