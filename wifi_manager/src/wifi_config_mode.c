/**
 * @file wifi_config_mode.c
 * @brief WIFI配置模式管理模块
 * 
 * 管理配置模式切换，支持AP模式、Web模式和BLE模式
 * 
 * 配置模式类型：
 * - WIFI_CONFIG_MODE_AP: AP配网模式，启动SoftAP热点
 * - WIFI_CONFIG_MODE_WEB: Web配网模式，启动SoftAP热点和Web服务器
 * - WIFI_CONFIG_MODE_BLE: 蓝牙配网模式，预留接口待后续实现
 * - WIFI_CONFIG_MODE_NONE: 无配置模式
 */

#include "wifi_manager.h"
#include <esp_log.h>

/* 日志标签 */
static const char *TAG = "wifi_config_mode";

/* 全局配置（外部引用） */
extern wifi_manager_config_t s_config;

/* 当前配置模式（外部引用） */
extern wifi_config_mode_t s_current_config_mode;

/**
 * @brief 启动配置模式
 * 
 * @param mode 配置模式类型
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 启动流程：
 * 1. 断开当前WIFI连接
 * 2. 设置当前配置模式
 * 3. 根据模式类型启动对应服务：
 *    - WIFI_CONFIG_MODE_AP: 启动SoftAP热点
 *    - WIFI_CONFIG_MODE_WEB: 启动SoftAP热点和Web服务器
 *    - WIFI_CONFIG_MODE_BLE: 蓝牙配网模式（预留）
 *    - WIFI_CONFIG_MODE_NONE: 不做任何操作
 */
esp_err_t wifi_config_mode_start(wifi_config_mode_t mode) {
    ESP_LOGI(TAG, "Starting config mode: %d", mode);

    /* 断开当前WIFI连接（如果已连接） */
    wifi_disconnect();

    /* 先停止当前的配置模式服务 */
    if (s_current_config_mode != WIFI_CONFIG_MODE_NONE) {
        ESP_LOGI(TAG, "Stopping previous config mode: %d", s_current_config_mode);
        wifi_config_mode_stop();
    }

    /* 设置当前配置模式 */
    s_current_config_mode = mode;

    /* 根据模式类型启动对应服务 */
    switch (mode) {
        case WIFI_CONFIG_MODE_AP:
        case WIFI_CONFIG_MODE_WEB:
            ESP_LOGI(TAG, "Starting SoftAP with SSID: %s", s_config.softap_config.ssid);
            wifi_manager_softap_start(&s_config.softap_config);
            
            if (mode == WIFI_CONFIG_MODE_WEB) {
                ESP_LOGI(TAG, "Starting Web server");
                wifi_webserver_start();
            }
            break;

        case WIFI_CONFIG_MODE_BLE:
            ESP_LOGW(TAG, "BLE config mode not implemented yet");
            break;

        case WIFI_CONFIG_MODE_NONE:
        default:
            ESP_LOGW(TAG, "No config mode specified");
            break;
    }

    ESP_LOGI(TAG, "Config mode %d started successfully", mode);
    return ESP_OK;
}

/**
 * @brief 停止配置模式
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 停止流程：
 * 1. 根据当前配置模式停止对应服务：
 *    - WIFI_CONFIG_MODE_AP: 停止SoftAP热点
 *    - WIFI_CONFIG_MODE_WEB: 停止Web服务器和SoftAP热点
 *    - WIFI_CONFIG_MODE_BLE: 蓝牙配网模式（预留）
 *    - WIFI_CONFIG_MODE_NONE: 不做任何操作
 * 2. 重置配置模式为NONE
 */
esp_err_t wifi_config_mode_stop(void) {
    ESP_LOGI(TAG, "Stopping config mode: %d", s_current_config_mode);

    /* 根据当前配置模式停止对应服务 */
    switch (s_current_config_mode) {
        case WIFI_CONFIG_MODE_AP:
        case WIFI_CONFIG_MODE_WEB:
            if (s_current_config_mode == WIFI_CONFIG_MODE_WEB) {
                /* 停止Web服务器 */
                wifi_webserver_stop();
            }
            /* 停止SoftAP热点 */
            wifi_manager_softap_stop();
            break;

        case WIFI_CONFIG_MODE_BLE:
            ESP_LOGW(TAG, "BLE config mode not implemented yet");
            break;

        case WIFI_CONFIG_MODE_NONE:
        default:
            /* 无配置模式：不做任何操作 */
            break;
    }

    /* 重置配置模式为NONE */
    s_current_config_mode = WIFI_CONFIG_MODE_NONE;

    return ESP_OK;
}

/**
 * @brief 获取当前配置模式类型
 * 
 * @return wifi_config_mode_t 当前配置模式类型
 */
wifi_config_mode_t wifi_config_mode_get_current(void) {
    return s_current_config_mode;
}
