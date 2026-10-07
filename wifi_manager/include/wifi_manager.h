/**
 * @file     wifi_manager.h
 * @brief    WiFi 配网组件 —— 统一管理 STA 连接与 AP 配网
 *
 * 功能：
 *   - 初始化后按 mode 选择行为：
 *       WIFI_MGR_MODE_AUTO      : 先尝试 STA 连接；失败后自动进 AP+Web 配网
 *       WIFI_MGR_MODE_CONNECT   : 只尝试 STA 连接（永不进配网）
 *       WIFI_MGR_MODE_CONFIG    : 只启动 AP+Web 配网（不连 STA）
 *   - 配网成功连接后通过 event_cb 通知上层
 *   - 提供扫描、状态查询、手动强制配网入口
 *
 * 修改：2026-10-06 合并原 wifi_prov 的事件回调机制，统一到本组件
 */
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>
#include <esp_wifi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 工作模式 ==================== */

/**
 * @brief WiFi 管理器工作模式
 */
typedef enum {
    WIFI_MGR_MODE_AUTO,      /* 先连 STA，失败后自动进 AP+Web 配网（推荐）*/
    WIFI_MGR_MODE_CONNECT,   /* 只连 STA，不进配网 */
    WIFI_MGR_MODE_CONFIG,    /* 只启动 AP+Web 配网，不连 STA */
} wifi_manager_mode_t;

/* ==================== 事件类型 ==================== */

typedef enum {
    WIFI_MGR_EV_AP_STARTED,      /* SoftAP + Web 页面已就绪 */
    WIFI_MGR_EV_CONNECTING,      /* 开始尝试 STA 连接 */
    WIFI_MGR_EV_CONNECTED,       /* STA 已关联热点（还未拿到 IP）*/
    WIFI_MGR_EV_GOT_IP,          /* 拿到 IP，连接成功 */
    WIFI_MGR_EV_DISCONNECTED,    /* STA 断开连接 */
    WIFI_MGR_EV_FALLBACK,        /* 重试耗尽，自动回落 AP 配网模式 */
    WIFI_MGR_EV_SAVED,           /* 用户从 Web 页提交了新的 WiFi 配置 */
} wifi_mgr_event_t;

/* ==================== 回调函数 ==================== */

/**
 * @brief  WiFi 状态变化回调
 *
 * @param event  事件类型
 * @param ctx    注册时传入的用户上下文
 */
typedef void (*wifi_manager_event_cb_t)(wifi_mgr_event_t event, void *ctx);

/* ==================== 状态结构体 ==================== */

typedef enum {
    WIFI_SIGNAL_NONE = 0,
    WIFI_SIGNAL_VERY_WEAK,
    WIFI_SIGNAL_WEAK,
    WIFI_SIGNAL_MEDIUM,
    WIFI_SIGNAL_STRONG,
} wifi_signal_level_t;

typedef struct {
    bool     connected;
    char     ssid[33];
    int8_t   rssi;
    wifi_signal_level_t signal_level;
    uint32_t connect_time_s;   /* 连接时长（秒）*/
    char     ip_address[16];
} wifi_status_t;

/* ==================== 配置结构体 ==================== */

typedef struct {
    wifi_manager_mode_t mode;           /* 工作模式 */
    uint8_t           max_retry;        /* STA 连接最大重试次数（0=无限）*/
    uint32_t          retry_interval_ms;/* 每次重试间隔 */
    bool              auto_reconnect;   /* 断线后是否自动重连 */
    bool              enable_ntp;       /* 连上后是否启动 NTP */
    const char       *ntp_server;       /* NTP 服务器地址 */
    const char       *fallback_ssid;    /* 配网热点 SSID（默认 "XIAOLE_WIFI"）*/
    const char       *fallback_pw;      /* 配网热点密码（默认 "12345678"）*/
    uint8_t           fallback_channel; /* 配网热点信道（默认 6）*/

    /* 回调 */
    wifi_manager_event_cb_t event_cb;   /* 可选，可为 NULL */
    void                   *event_cb_ctx;
} wifi_manager_config_t;

/* ==================== 核心 API ==================== */

/**
 * @brief  初始化 WiFi 管理器
 *
 * 根据 mode 执行不同流程：
 *   AUTO  : 加载 NVS 配置 → 尝试 STA 连接 → 失败则进 AP+Web
 *   CONNECT: 加载 NVS 配置 → 只尝试 STA 连接
 *   CONFIG : 直接启动 AP+Web 配网
 *
 * @param config 配置参数，不可为 NULL
 * @return ESP_OK 成功
 */
esp_err_t wifi_manager_init(const wifi_manager_config_t *config);

/**
 * @brief  反初始化（停止 WiFi）
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief  注册事件回调
 *
 * @note   也可在 config.event_cb 中一次性设置；此函数供动态替换用
 */
void wifi_manager_set_event_cb(wifi_manager_event_cb_t cb, void *ctx);

/* ==================== 状态查询 ==================== */

esp_err_t wifi_manager_get_status(wifi_status_t *status);
bool      wifi_manager_is_connected(void);
wifi_signal_level_t wifi_manager_rssi_to_level(int8_t rssi);

/* ==================== STA 连接管理 ==================== */

/**
 * @brief  强制发起一次 STA 连接（可用于手动重连）
 */
esp_err_t wifi_manager_sta_connect(void);

/**
 * @brief  断开当前 STA 连接
 */
esp_err_t wifi_manager_sta_disconnect(void);

/* ==================== AP / 配网管理 ==================== */

/**
 * @brief  手动启动 AP + Web 配网页面
 *
 * @note   会清除旧 NVS 配置，防止下次上电直连旧 AP
 */
esp_err_t wifi_manager_start_config_portal(void);

/**
 * @brief  停止配网（回到 STA 模式）
 */
esp_err_t wifi_manager_stop_config_portal(void);

/**
 * @brief  获取当前配置模式
 */
wifi_manager_mode_t wifi_manager_get_mode(void);

/* ==================== 扫描（同步阻塞） ==================== */

/**
 * @brief  同步阻塞扫描，等待完成后再返回结果（约 12-15 秒）
 *
 * @param buf        输出：ap_record 数组（调用方需分配，最多 max_count 条）
 * @param max_count  输入：数组容量；输出：实际扫描到的 AP 数量
 * @return ESP_OK / 其他错误码
 */
esp_err_t wifi_manager_scan_all(wifi_ap_record_t *buf, uint16_t max_count, uint16_t *out_count);

/* ==================== NVS 配置 ==================== */

esp_err_t wifi_manager_save_config(const char *ssid, const char *password);
esp_err_t wifi_manager_load_config(char *ssid, char *password, size_t ssid_len, size_t pw_len);
esp_err_t wifi_manager_clear_config(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */
