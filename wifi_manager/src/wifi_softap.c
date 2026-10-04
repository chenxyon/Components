/**
 * @file wifi_softap.c
 * @brief WIFI SoftAP功能模块
 * 
 * 创建热点供手机连接，提供配置入口
 * 
 * SoftAP配置参数：
 * - SSID: 热点名称
 * - Password: 热点密码（为空则开放）
 * - Channel: 信道（默认1）
 * - Hidden: 是否隐藏（默认否）
 * - Max connections: 最大连接数（默认4）
 * - Auth mode: 认证模式（默认WPA/WPA2 PSK）
 */

#include "wifi_manager.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <string.h>

/* 日志标签 */
static const char *TAG = "wifi_softap";

/* 当前SoftAP配置 */
static wifi_softap_config_t s_current_softap_config;

/**
 * @brief 启动SoftAP热点
 * 
 * @param config SoftAP配置参数
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 启动流程：
 * 1. 验证配置参数
 * 2. 保存配置
 * 3. 设置WIFI模式为AP
 * 4. 配置AP参数（SSID、密码、信道、认证模式等）
 * 5. 设置WIFI配置并启动
 */
esp_err_t wifi_manager_softap_start(const wifi_softap_config_t *config) {
    /* 验证配置参数 */
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* 保存配置 */
    memcpy(&s_current_softap_config, config, sizeof(wifi_softap_config_t));

    /* 设置WIFI模式为APSTA（同时支持AP和STA） */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    /* 禁用STA自动连接，避免影响SoftAP稳定性 */
    wifi_config_t sta_config = {.sta = {.ssid = ""}};
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));

    /* 配置AP参数 */
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "",                              /* SSID */
            .password = "",                          /* 密码 */
            .channel = config->channel,              /* 信道 */
            .ssid_hidden = config->hidden,           /* 是否隐藏 */
            .max_connection = 4,                     /* 最大连接数 */
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,      /* 认证模式 */
        }
    };

    /* 复制SSID */
    strncpy((char*)ap_config.ap.ssid, config->ssid, sizeof(ap_config.ap.ssid) - 1);
    
    /* 复制密码 */
    strncpy((char*)ap_config.ap.password, config->password, sizeof(ap_config.ap.password) - 1);

    /* 如果密码为空，设置为开放模式 */
    if (strlen(config->password) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    /* 设置WIFI配置 */
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    
    /* 启动WIFI */
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started: SSID=%s, Channel=%d", config->ssid, config->channel);

    return ESP_OK;
}

/**
 * @brief 停止SoftAP热点
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 停止流程：
 * 1. 停止WIFI驱动
 */
esp_err_t wifi_manager_softap_stop(void) {
    /* 停止WIFI驱动 */
    ESP_ERROR_CHECK(esp_wifi_stop());

    ESP_LOGI(TAG, "SoftAP stopped");

    return ESP_OK;
}

/**
 * @brief 获取SoftAP状态
 * 
 * @param status 状态结构体指针（输出参数）
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 获取的状态信息：
 * - SSID: 当前热点名称
 * - IP地址: AP接口的IP地址
 */
esp_err_t wifi_softap_get_status(wifi_status_t *status) {
    if (status == NULL) {
        ESP_LOGE(TAG, "Status is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(status, 0, sizeof(wifi_status_t));

    wifi_config_t ap_config;
    if (esp_wifi_get_config(WIFI_IF_AP, &ap_config) == ESP_OK) {
        strncpy(status->ssid, (const char*)ap_config.ap.ssid, sizeof(status->ssid) - 1);
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), 
                              &ip_info) == ESP_OK) {
        sprintf(status->ip_address, IPSTR, IP2STR(&ip_info.ip));
    }

    return ESP_OK;
}
