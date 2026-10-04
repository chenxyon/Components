/**
 * @file wifi_time_sync.c
 * @brief WIFI管理器 - NTP时间同步模块
 * 
 * 实现NTP时间同步功能：
 * - 通过SNTP协议同步系统时间
 * - 支持自定义NTP服务器和同步间隔
 * - 在获取IP地址后自动启动时间同步
 */

#include "wifi_manager.h"
#include <esp_log.h>
#include <esp_sntp.h>
#include <string.h>

/* 日志标签 */
static const char *TAG = "wifi_time_sync";

/* 时间同步配置 */
static wifi_time_sync_config_t s_sync_config = {
    .enabled = false,
    .sync_interval_min = 60,
    .ntp_server = "cn.pool.ntp.org"
};

/* 同步启用标志 */
static bool s_sync_enabled = false;

/* 时间同步回调 */
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized successfully");
}

/**
 * @brief 启用时间同步功能
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 启用流程：
 * 1. 检查是否已启用
 * 2. 配置SNTP参数
 * 3. 启动SNTP服务
 */
esp_err_t wifi_time_sync_enable(void)
{
    /* 检查是否已启用 */
    if (s_sync_enabled) {
        ESP_LOGW(TAG, "Time sync already enabled");
        return ESP_OK;
    }

    /* 检查是否已启用时间同步功能 */
    if (!s_sync_config.enabled) {
        ESP_LOGW(TAG, "Time sync is disabled in config");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting time sync, server=%s, interval=%d min",
             s_sync_config.ntp_server, s_sync_config.sync_interval_min);

    /* 配置SNTP */
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, s_sync_config.ntp_server);
    esp_sntp_set_sync_interval(s_sync_config.sync_interval_min * 60 * 1000);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    /* 启动SNTP */
    esp_sntp_init();

    s_sync_enabled = true;
    ESP_LOGI(TAG, "Time sync enabled");

    return ESP_OK;
}

/**
 * @brief 禁用时间同步功能
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_time_sync_disable(void)
{
    /* 检查是否已启用 */
    if (!s_sync_enabled) {
        ESP_LOGW(TAG, "Time sync not enabled");
        return ESP_OK;
    }

    /* 停止SNTP */
    esp_sntp_stop();

    s_sync_enabled = false;
    ESP_LOGI(TAG, "Time sync disabled");

    return ESP_OK;
}

/**
 * @brief 设置时间同步间隔
 * 
 * @param interval_min 同步间隔（分钟）
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_time_sync_set_interval(uint32_t interval_min)
{
    /* 验证参数 */
    if (interval_min < 1) {
        ESP_LOGE(TAG, "Invalid interval: %d, must be >= 1", interval_min);
        return ESP_ERR_INVALID_ARG;
    }

    /* 保存配置 */
    s_sync_config.sync_interval_min = interval_min;

    ESP_LOGI(TAG, "Time sync interval updated: %d minutes", interval_min);

    /* 如果已启用同步，更新SNTP间隔 */
    if (s_sync_enabled) {
        sntp_set_sync_interval(interval_min * 60 * 1000);
        sntp_restart();
    }

    return ESP_OK;
}
