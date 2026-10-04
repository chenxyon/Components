/**
 * @file wifi_config.c
 * @brief WIFI配置管理模块
 * 
 * 实现WIFI配置的保存、加载和清除功能：
 * - 使用NVS（Non-Volatile Storage）持久化存储WIFI配置
 * - 保存SSID、密码和配置状态
 * - 支持配置的读取和清除
 */

#include "wifi_manager.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>
#include <string.h>

/* 日志标签 */
static const char *TAG = "wifi_config";

/* NVS命名空间 */
#define NVS_NAMESPACE "wifi_config"

/* NVS键名 */
#define NVS_KEY_SSID    "ssid"
#define NVS_KEY_PASSWORD "password"
#define NVS_KEY_SAVED   "saved"
#define NVS_KEY_VERSION "version"

/* 当前固件版本号（修改此值可触发配置重置） */
#define WIFI_CONFIG_VERSION 1

/**
 * @brief 保存WIFI配置到NVS
 * 
 * @param config WIFI配置参数
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 保存流程：
 * 1. 检查参数有效性
 * 2. 打开NVS命名空间
 * 3. 设置SSID键值对
 * 4. 设置密码键值对
 * 5. 设置保存标志键值对
 * 6. 提交更改并关闭NVS
 */
esp_err_t wifi_config_save(const wifi_saved_config_t *config) {
    /* 检查参数 */
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* 打开NVS命名空间 */
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    /* 保存SSID */
    err = nvs_set_str(nvs_handle, NVS_KEY_SSID, config->ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    /* 保存密码 */
    err = nvs_set_str(nvs_handle, NVS_KEY_PASSWORD, config->password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    /* 保存保存标志 */
    uint8_t saved = config->saved ? 1 : 0;
    err = nvs_set_u8(nvs_handle, NVS_KEY_SAVED, saved);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save saved flag: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    /* 保存版本号 */
    err = nvs_set_u32(nvs_handle, NVS_KEY_VERSION, WIFI_CONFIG_VERSION);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save version: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    /* 提交更改 */
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "WIFI config saved: SSID=%s", config->ssid);
    return ESP_OK;
}

/**
 * @brief 从NVS加载WIFI配置
 * 
 * @param config WIFI配置结构体指针（输出参数）
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 加载流程：
 * 1. 检查参数有效性
 * 2. 打开NVS命名空间
 * 3. 读取保存标志
 * 4. 如果有保存的配置，读取SSID和密码
 * 5. 关闭NVS
 */
esp_err_t wifi_config_load(wifi_saved_config_t *config) {
    /* 检查参数 */
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* 初始化结构体为默认值 */
    memset(config, 0, sizeof(wifi_saved_config_t));
    config->saved = false;

    /* 打开NVS命名空间 */
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    /* 读取保存标志 */
    uint8_t saved = 0;
    err = nvs_get_u8(nvs_handle, NVS_KEY_SAVED, &saved);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No saved config found");
        nvs_close(nvs_handle);
        return err;
    }

    config->saved = (saved != 0);

    if (config->saved) {
        uint32_t saved_version = 0;
        esp_err_t version_err = nvs_get_u32(nvs_handle, NVS_KEY_VERSION, &saved_version);
        if (version_err != ESP_OK || saved_version != WIFI_CONFIG_VERSION) {
            ESP_LOGW(TAG, "Config version mismatch (saved=%d, current=%d), clearing config", 
                     saved_version, WIFI_CONFIG_VERSION);
            nvs_close(nvs_handle);
            wifi_config_clear();
            memset(config, 0, sizeof(wifi_saved_config_t));
            config->saved = false;
            return ESP_ERR_NOT_FOUND;
        }
    }

    /* 如果有保存的配置，读取SSID和密码 */
    if (config->saved) {
        size_t len = sizeof(config->ssid);
        err = nvs_get_str(nvs_handle, NVS_KEY_SSID, config->ssid, &len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load SSID: %s", esp_err_to_name(err));
            nvs_close(nvs_handle);
            return err;
        }

        len = sizeof(config->password);
        err = nvs_get_str(nvs_handle, NVS_KEY_PASSWORD, config->password, &len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load password: %s", esp_err_to_name(err));
            nvs_close(nvs_handle);
            return err;
        }
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "WIFI config loaded: saved=%d, SSID=%s", config->saved, config->ssid);
    return ESP_OK;
}

/**
 * @brief 清除保存的WIFI配置
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_config_clear(void) {
    /* 打开NVS命名空间 */
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    /* 清除所有键值对 */
    err = nvs_erase_all(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    /* 提交更改 */
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "WIFI config cleared");
    return ESP_OK;
}
