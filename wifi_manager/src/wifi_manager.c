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
#include "captive_portal.h"

#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_timer.h>
#include <lwip/apps/sntp.h>

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static const char *TAG = "wifi_mgr";

/* ==================== 日志环形缓存（供 WEB /api/logs 查看） ==================== */
#define WEBLOG_LINES  120
#define WEBLOG_LEN    160
static char      s_weblog[WEBLOG_LINES][WEBLOG_LEN];
static uint16_t  s_weblog_head  = 0;   /* 下一次写入位置 */
static uint16_t  s_weblog_count = 0;
static SemaphoreHandle_t s_weblog_lock = NULL;
static vprintf_like_t    s_orig_vprintf = NULL;

static void weblog_push(const char *line)
{
    if (s_weblog_lock == NULL) return;
    if (xSemaphoreTake(s_weblog_lock, pdMS_TO_TICKS(20)) != pdTRUE) return;
    strncpy(s_weblog[s_weblog_head], line, WEBLOG_LEN - 1);
    s_weblog[s_weblog_head][WEBLOG_LEN - 1] = '\0';
    s_weblog_head = (s_weblog_head + 1) % WEBLOG_LINES;
    if (s_weblog_count < WEBLOG_LINES) s_weblog_count++;
    xSemaphoreGive(s_weblog_lock);
}

/* 拦截所有 ESP_LOGx：先存环形缓存，再交给原 vprintf 输出串口 */
static int weblog_vprintf(const char *fmt, va_list args)
{
    char tmp[WEBLOG_LEN + 32];
    va_list cp;
    va_copy(cp, args);
    vsnprintf(tmp, sizeof(tmp), fmt, cp);
    va_end(cp);
    /* 去掉末尾换行，WEB 显示更整齐 */
    size_t n = strlen(tmp);
    while (n > 0 && (tmp[n - 1] == '\n' || tmp[n - 1] == '\r')) tmp[--n] = '\0';
    weblog_push(tmp);
    if (s_orig_vprintf) return s_orig_vprintf(fmt, args);
    return vprintf(fmt, args);
}

/* 供 webserver 的 /api/logs 调用：把缓存拼成 JSON 数组 */
esp_err_t wifi_manager_get_weblog(char *out, size_t out_len, uint16_t max_lines)
{
    if (!out || out_len == 0) return ESP_ERR_INVALID_ARG;
    if (s_weblog_lock == NULL) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_weblog_lock, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    uint16_t n = s_weblog_count < max_lines ? s_weblog_count : max_lines;
    uint16_t start = (s_weblog_head + WEBLOG_LINES - n) % WEBLOG_LINES;
    size_t off = 0;
    off += snprintf(out + off, out_len - off, "[");
    for (uint16_t i = 0; i < n && off + 8 < out_len; i++) {
        uint16_t idx = (start + i) % WEBLOG_LINES;
        /* 简单转义双引号和反斜杠 */
        char esc[WEBLOG_LEN * 2];
        size_t e = 0;
        for (const char *p = s_weblog[idx]; *p && e + 2 < sizeof(esc); p++) {
            if (*p == '"' || *p == '\\') esc[e++] = '\\';
            esc[e++] = *p;
        }
        esc[e] = '\0';
        off += snprintf(out + off, out_len - off, "%s\"%s\"",
                        i ? "," : "", esc);
        if (off >= out_len - 4) break;
    }
    snprintf(out + off, out_len - off, "]");
    xSemaphoreGive(s_weblog_lock);
    return ESP_OK;
}

/* ==================== 内部状态 ==================== */
static bool             s_ready       = false;
static wifi_manager_mode_t s_mode     = WIFI_MGR_MODE_AUTO;

static uint8_t          s_retry_count = 0;
static bool             s_ap_started  = false;
static int64_t          s_connect_start_us = 0;  /* 本次连接计时起点（sta_connect 调用时） */

/* 回调 */
static wifi_manager_event_cb_t s_event_cb    = NULL;
static void                   *s_event_cb_ctx = NULL;

/* 配置（拷贝自入参） */
static wifi_manager_config_t s_cfg;

/* ==================== 事件派发 ==================== */
static void emit_event(wifi_mgr_event_t ev)
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
        emit_event(WIFI_MGR_EV_CONNECTING);
        ESP_LOGI(TAG, "[1/4] STA 已启动，开始扫描/认证…");
        break;

    case WIFI_EVENT_STA_CONNECTED:
        emit_event(WIFI_MGR_EV_CONNECTED);
        {
            long elapsed_ms = s_connect_start_us ?
                (long)((esp_timer_get_time() - s_connect_start_us) / 1000) : -1;
            ESP_LOGI(TAG, "[2/4] 已关联热点，等待获取 IP …（+%ld ms）", elapsed_ms);
        }
        break;

    case WIFI_EVENT_STA_DISCONNECTED: {
        emit_event(WIFI_MGR_EV_DISCONNECTED);
        int reason = -1;
        if (data) {
            reason = ((wifi_event_sta_disconnected_t *)data)->reason;
        }
        ESP_LOGW(TAG, "STA 断开，reason=%d", reason);
        if (s_mode == WIFI_MGR_MODE_AUTO && !s_ap_started) {
            s_retry_count++;
            ESP_LOGW(TAG, "断开，重试 %u/%u",
                     s_retry_count, s_cfg.max_retry);
            if (s_retry_count >= s_cfg.max_retry && s_cfg.max_retry != 0) {
                ESP_LOGW(TAG, "重试耗尽，进入配网模式");
                emit_event(WIFI_MGR_EV_FALLBACK);
                s_ap_started = true;
                wifi_manager_start_config_portal();
            } else if (s_cfg.auto_reconnect) {
                vTaskDelay(pdMS_TO_TICKS(s_cfg.retry_interval_ms));
                esp_wifi_connect();
            }
        } else if (s_cfg.auto_reconnect) {
            /* APSTA 配网期间也重试，但加次数上限，避免密码错误时无限空转。
             * 事件任务里不能 vTaskDelay（会阻塞整个事件循环），直接重连。 */
            s_retry_count++;
            if (s_cfg.max_retry != 0 && s_retry_count > s_cfg.max_retry) {
                ESP_LOGE(TAG, "STA 连接失败已达 %u 次，请检查 SSID/密码后重试",
                         s_retry_count - 1);
                return;
            }
            esp_err_t rc = esp_wifi_connect();
            if (rc != ESP_OK) {
                ESP_LOGW(TAG, "重连调用失败: %s", esp_err_to_name(rc));
            }
        }
        break;
    }

    case WIFI_EVENT_AP_START:
        s_ap_started = true;
        emit_event(WIFI_MGR_EV_AP_STARTED);
        break;

    default:
        break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t base,
                             int32_t id, void *data)
{
    (void)arg;
    if (base != IP_EVENT || id != IP_EVENT_STA_GOT_IP) return;

    s_retry_count = 0;
    emit_event(WIFI_MGR_EV_GOT_IP);

    /* 打印 IP + 本次连接总耗时 */
    {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        long elapsed_ms = s_connect_start_us ?
            (long)((esp_timer_get_time() - s_connect_start_us) / 1000) : -1;
        ESP_LOGI(TAG, "[3/4] 已获取 IP：" IPSTR "（本次连接耗时 %ld ms）",
                 IP2STR(&evt->ip_info.ip), elapsed_ms);
        s_connect_start_us = 0;
    }

    /* [4/4] 关 AP：连上后 AP 留着只会占信道、干扰 STA，且配网已完成。
     * 注意：只切 mode，不调 stop/start——stop 会把刚连上的 STA 也掐断。 */
    if (s_ap_started) {
        ESP_LOGI(TAG, "[4/4] STA 已连上，开始关闭 AP…");
        captive_portal_stop();
        ESP_LOGI(TAG, "[4/4] Captive Portal/DNS 已停止");
        esp_err_t wrc = esp_wifi_set_mode(WIFI_MODE_STA);
        if (wrc != ESP_OK) {
            ESP_LOGW(TAG, "[4/4] 关闭 AP 失败: %s", esp_err_to_name(wrc));
        } else {
            ESP_LOGI(TAG, "[4/4] AP 已关闭，当前纯 STA 模式");
        }
        s_ap_started = false;
        /* Webserver 监听 0.0.0.0，mode 切换不影响，已绑定的 server 不用重起 */
        ESP_LOGI(TAG, "[4/4] Webserver 保留，STA IP 上可继续访问 /success、/api/logs");
    } else {
        ESP_LOGI(TAG, "已连上路由器（AP 本来就没开）");
    }

    /* STA IP 上也要能打开 /success、/api/logs、/api/status。
     * 注意：直连旧 AP 上电时根本没进过配网，Web 服务从未启动，
     * 这里补起一次；只起 HTTP，不绑 DNS/Captive（STA 下不需要劫持）。 */
    if (wifi_webserver_handle() == NULL) {
        ESP_LOGI(TAG, "[4/4] STA 模式补起 Web 服务（仅 HTTP）…");
        if (wifi_webserver_start_http_only() != ESP_OK) {
            ESP_LOGW(TAG, "[4/4] Web 服务补起失败，STA IP 上查不了 /success");
        }
    }

    /* NTP：连上后启动时间同步 */
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
    /* 接上配置里带的回调（也可用 wifi_manager_set_event_cb 动态替换） */
    s_event_cb     = config->event_cb;
    s_event_cb_ctx = config->event_cb_ctx;

    /* NVS */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 异常，擦除重建");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(err);

    /* WiFi 配置：刷固件、重启都不清除，始终保留并自动连接上次保存的 AP。
     * 开发期调试如需强制进配网，请手动调用 wifi_manager_clear_config() 后重启，
     * 或调用 wifi_manager_start_config_portal()。 */
    ESP_LOGI(TAG, "保留 NVS WiFi 配置");

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

    /* 安装日志拦截：此后所有 ESP_LOGx 同时进环形缓存，供 WEB /api/logs 查看 */
    if (s_weblog_lock == NULL) {
        s_weblog_lock = xSemaphoreCreateMutex();
    }
    if (s_orig_vprintf == NULL) {
        s_orig_vprintf = esp_log_set_vprintf(weblog_vprintf);
    }
    ESP_LOGI(TAG, "WiFi 管理器初始化完成，mode=%d（WEB 日志缓存已启用）", (int)s_mode);

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
    s_connect_start_us = esp_timer_get_time();  /* [0/4] 连接计时起点 */
    ESP_LOGI(TAG, "[0/4] 发起 STA 连接…");
    /* 保持 AP 运行：APSTA 下 STA 可直接连接，停 AP 会让手机掉线、
     * 配网页不可访问，且 stop 后未 start 会导致 connect 直接失败。 */

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
    /* 确保 APSTA 运行（扫描/配网后可能只剩 AP），再下发 STA 配置并连接 */
    esp_err_t mrc = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (mrc != ESP_OK && mrc != ESP_ERR_WIFI_STATE) {
        ESP_LOGW(TAG, "set_mode APSTA 失败: %s", esp_err_to_name(mrc));
    }
    esp_err_t src = esp_wifi_start();
    if (src != ESP_OK && src != ESP_ERR_WIFI_STATE) {
        ESP_LOGW(TAG, "wifi_start 失败: %s", esp_err_to_name(src));
    }
    s_ap_started = true;
    ESP_LOGI(TAG, "开始连接 STA: %s (密码长度 %d)", ssid, (int)strlen(pw));
    ESP_LOGI(TAG, "下发 STA 配置…");
    esp_wifi_disconnect();
    esp_err_t crc = esp_wifi_set_config(WIFI_IF_STA, &wc);
    if (crc != ESP_OK) {
        ESP_LOGE(TAG, "STA 配置下发失败: %s", esp_err_to_name(crc));
        return crc;
    }
    crc = esp_wifi_connect();
    if (crc != ESP_OK) {
        ESP_LOGE(TAG, "STA 连接发起失败: %s", esp_err_to_name(crc));
        return crc;
    }
    ESP_LOGI(TAG, "STA 连接已发起，等待关联…");
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

    /* 注意：不要在这里清 NVS！保存的密码是 STA 连接的唯一凭证，
     * 清掉会导致连接失败后无法重试、重启后也无法自动连接。 */
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

/* ==================== 同步阻塞扫描 ==================== */
/*
 * 每次调用都同步执行扫描，等待完成后再返回结果。
 * 优点：无竞态、无轮询、网络切换时也能可靠工作。
 * 缺点：调用时会阻塞约 12-15 秒（扫描超时）。
 */
esp_err_t wifi_manager_scan_all(wifi_ap_record_t *buf, uint16_t max_count, uint16_t *out_count)
{
    if (!buf || !out_count) return ESP_ERR_INVALID_ARG;

    esp_wifi_scan_stop();

    /* 注意：扫描时保持 AP 运行。APSTA 模式支持后台扫描，
     * 停 AP 会掐断配网手机的 HTTP 连接，导致前端报"请求失败"。 */
    wifi_scan_config_t cfg = {
        .ssid       = NULL,
        .bssid      = NULL,
        .channel    = 0,
        .show_hidden = false,
        .scan_type  = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time  = { .active.min = 60, .active.max = 180 },
    };

    int64_t scan_start = esp_timer_get_time();
    ESP_LOGI(TAG, "开始扫描可用 AP…");
    esp_err_t err = esp_wifi_scan_start(&cfg, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "扫描启动失败: %s", esp_err_to_name(err));
        return err;
    }

    uint16_t n = max_count < 20 ? max_count : 20;
    err = esp_wifi_scan_get_ap_records(&n, buf);
    *out_count = (err == ESP_OK) ? n : 0;
    ESP_LOGI(TAG, "扫描完成，发现 %u 个 AP（耗时 %ld ms）", *out_count,
             (long)((esp_timer_get_time() - scan_start) / 1000));
    return err;
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
