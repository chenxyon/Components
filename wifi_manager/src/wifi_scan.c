/**
 * @file wifi_scan.c
 * @brief WIFI扫描功能模块
 * 
 * 扫描周围可用的WIFI热点，获取SSID、信号强度和加密方式
 * 
 * 扫描流程：
 * 1. 设置扫描配置（SSID为空表示扫描所有AP）
 * 2. 启动扫描
 * 3. 获取扫描结果
 * 4. 解析结果并存储到扫描结果数组
 * 5. 提供结果获取和释放接口
 */

#include "wifi_manager.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdlib.h>

/* 日志标签 */
static const char *TAG = "wifi_scan";

/* 最大扫描结果数量 */
#define MAX_SCAN_RESULTS 20

/* 扫描结果数组 */
static wifi_scan_result_t *s_scan_results = NULL;

/* 扫描结果数量 */
static uint16_t s_scan_count = 0;

/**
 * @brief 启动WIFI扫描
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 扫描流程：
 * 1. 配置扫描参数（扫描所有AP）
 * 2. 启动扫描
 * 3. 等待扫描完成（阻塞方式）
 */
esp_err_t wifi_scan_start(void) {
    ESP_LOGI(TAG, "Starting WIFI scan...");

    /* 释放之前的扫描结果 */
    if (s_scan_results != NULL) {
        free(s_scan_results);
        s_scan_results = NULL;
    }
    s_scan_count = 0;

    /* 配置扫描参数 */
    wifi_scan_config_t scan_config = {
        .ssid = NULL,                     /* 扫描所有AP */
        .bssid = NULL,                    /* 不指定BSSID */
        .channel = 0,                     /* 扫描所有信道 */
        .show_hidden = true,              /* 显示隐藏AP */
        .scan_type = WIFI_SCAN_TYPE_ACTIVE, /* 主动扫描 */
        .scan_time = {
            .active = {
                .min = 120,               /* 最小主动扫描时间 */
                .max = 240                /* 最大主动扫描时间 */
            }
        }
    };

    /* 启动扫描 */
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    ESP_LOGI(TAG, "WIFI scan completed");

    return ESP_OK;
}

/**
 * @brief 获取扫描结果
 * 
 * @param results 扫描结果数组指针（输出参数）
 * @param count 结果数量指针（输出参数）
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 获取流程：
 * 1. 获取扫描结果数量
 * 2. 分配内存存储结果
 * 3. 复制扫描结果到数组
 * 4. 设置结果数量
 */
esp_err_t wifi_scan_get_results(wifi_scan_result_t **results, uint16_t *count) {
    /* 检查参数 */
    if (results == NULL || count == NULL) {
        ESP_LOGE(TAG, "Invalid parameters (results=%p, count=%p)", results, count);
        return ESP_ERR_INVALID_ARG;
    }

    /* 获取扫描结果数量 */
    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    
    /* 限制结果数量 */
    if (ap_count > MAX_SCAN_RESULTS) {
        ap_count = MAX_SCAN_RESULTS;
    }

    /* 分配内存存储结果 */
    wifi_ap_record_t *ap_records = malloc(ap_count * sizeof(wifi_ap_record_t));
    if (ap_records == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for scan results");
        return ESP_ERR_NO_MEM;
    }

    /* 获取扫描结果 */
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

    /* 分配扫描结果数组 */
    s_scan_results = malloc(ap_count * sizeof(wifi_scan_result_t));
    if (s_scan_results == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for scan results");
        free(ap_records);
        return ESP_ERR_NO_MEM;
    }

    /* 复制扫描结果 */
    for (uint16_t i = 0; i < ap_count; i++) {
        strncpy(s_scan_results[i].ssid, (const char*)ap_records[i].ssid, 
                sizeof(s_scan_results[i].ssid) - 1);
        s_scan_results[i].rssi = ap_records[i].rssi;
        s_scan_results[i].auth_mode = ap_records[i].authmode;
    }

    /* 设置结果数量 */
    s_scan_count = ap_count;

    /* 设置输出参数 */
    *results = s_scan_results;
    *count = s_scan_count;

    /* 释放临时内存 */
    free(ap_records);

    ESP_LOGI(TAG, "Got %d scan results", ap_count);

    return ESP_OK;
}

/**
 * @brief 释放扫描结果内存
 * 
 * @param results 扫描结果数组指针
 * 
 * 释放流程：
 * 1. 检查指针是否为空
 * 2. 释放内存
 * 3. 重置全局变量
 */
void wifi_scan_free_results(wifi_scan_result_t *results) {
    /* 检查指针 */
    if (results == NULL) {
        return;
    }

    /* 释放内存 */
    free(results);

    /* 重置全局变量 */
    if (s_scan_results == results) {
        s_scan_results = NULL;
        s_scan_count = 0;
    }

    ESP_LOGI(TAG, "Scan results freed");
}

/**
 * @brief 获取扫描结果数量
 * 
 * @return uint16_t 扫描结果数量
 */
uint16_t wifi_scan_get_result_count(void) {
    return s_scan_count;
}

/**
 * @brief 获取加密方式字符串描述
 * 
 * @param auth_mode 加密方式枚举值
 * @return const char* 加密方式字符串
 * 
 * 加密方式映射：
 * - WIFI_AUTH_OPEN: "Open"
 * - WIFI_AUTH_WEP: "WEP"
 * - WIFI_AUTH_WPA_PSK: "WPA-PSK"
 * - WIFI_AUTH_WPA2_PSK: "WPA2-PSK"
 * - WIFI_AUTH_WPA_WPA2_PSK: "WPA/WPA2-PSK"
 * - WIFI_AUTH_WPA2_ENTERPRISE: "WPA2-Enterprise"
 * - WIFI_AUTH_WPA3_PSK: "WPA3-PSK"
 * - WIFI_AUTH_WPA2_WPA3_PSK: "WPA2/WPA3-PSK"
 * - Others: "Unknown"
 */
const char *wifi_scan_get_auth_mode_str(wifi_auth_mode_t auth_mode) {
    switch (auth_mode) {
        case WIFI_AUTH_OPEN:
            return "Open";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2-PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2-Enterprise";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/WPA3-PSK";
        default:
            return "Unknown";
    }
}
