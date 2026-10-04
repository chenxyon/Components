/**
 * @file wifi_manager.c
 * @brief WIFI管理组件主文件
 * 
 * 实现核心逻辑：初始化、状态管理、事件处理、连接管理、状态查询
 * 
 * 主要功能：
 * - 初始化WIFI管理器，配置事件处理
 * - 处理WIFI连接和断开事件
 * - 管理自动重连和重试机制
 * - 查询WIFI连接状态和信号强度
 */

#include "wifi_manager.h"
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif_types.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

/* 日志标签 */
static const char *TAG = "wifi_manager";

/* WIFI管理器全局配置 */
wifi_manager_config_t s_config;

/* 初始化标志 */
static bool s_initialized = false;

/* WIFI连接开始时间（毫秒），用于计算连接时长 */
static uint32_t s_connect_start_time = 0;

/* 当前重试次数 */
static uint8_t s_retry_count = 0;

/* 当前配置模式 */
wifi_config_mode_t s_current_config_mode = WIFI_CONFIG_MODE_NONE;

/* 扫描状态 */
static bool s_scanning = false;
static bool s_scan_completed = false;
static wifi_ap_record_t s_scan_results[20];
static uint16_t s_scan_count = 0;

/* 前向声明 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
static esp_err_t wifi_init_sta(void);

/**
 * @brief 初始化WIFI管理器
 * 
 * @param config WIFI管理器配置参数
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 初始化流程：
 * 1. 检查是否已初始化
 * 2. 验证配置参数
 * 3. 初始化NVS、网络接口、事件循环
 * 4. 创建默认的STA和AP网络接口
 * 5. 初始化WIFI驱动
 * 6. 注册WIFI事件和IP事件处理器
 * 7. 加载保存的WIFI配置并尝试连接，或进入配置模式
 */
esp_err_t wifi_manager_init(const wifi_manager_config_t *config) {
    /* 检查是否已初始化 */
    if (s_initialized) {
        ESP_LOGW(TAG, "WIFI manager already initialized");
        return ESP_OK;
    }

    /* 验证配置参数 */
    if (config == NULL) {
        ESP_LOGE(TAG, "Config parameter is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* 保存配置参数 */
    memcpy(&s_config, config, sizeof(wifi_manager_config_t));

    /* 初始化NVS存储 */
    ESP_ERROR_CHECK(nvs_flash_init());

    /* 初始化网络接口 */
    ESP_ERROR_CHECK(esp_netif_init());

    /* 创建默认事件循环 */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 创建默认的STA网络接口 */
    esp_netif_create_default_wifi_sta();

    /* 创建默认的AP网络接口 */
    esp_netif_create_default_wifi_ap();

    /* 初始化WIFI驱动 */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* 注册WIFI事件处理器 */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                               &wifi_event_handler, NULL));

    /* 注册IP事件处理器（获取IP地址） */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                               &wifi_event_handler, NULL));

    /* 设置初始化标志 */
    s_initialized = true;
    ESP_LOGI(TAG, "WIFI manager initialized successfully");

    /* 尝试加载保存的WIFI配置 */
    wifi_saved_config_t wifi_cfg;
    if (wifi_config_load(&wifi_cfg) == ESP_OK && wifi_cfg.saved) {
        /* 有保存的配置，尝试连接 */
        ESP_LOGI(TAG, "Found saved WIFI config, trying to connect to SSID: %s", wifi_cfg.ssid);
        ESP_ERROR_CHECK(wifi_connect());
    } else {
        /* 无保存的配置，进入配置模式 */
        ESP_LOGI(TAG, "No saved WIFI config found, entering config mode: %d", 
                 s_config.default_config_mode);
        ESP_ERROR_CHECK(wifi_config_mode_start(s_config.default_config_mode));
    }

    return ESP_OK;
}

/**
 * @brief WIFI事件处理器
 * 
 * @param arg 用户参数
 * @param event_base 事件基类
 * @param event_id 事件ID
 * @param event_data 事件数据
 * 
 * 处理WIFI连接、断开、获取IP等事件
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "STA started");
                break;

            case WIFI_EVENT_STA_CONNECTED:
                s_connect_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                ESP_LOGI(TAG, "Connected to WIFI");
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "Disconnected from WIFI");
                if (s_config.auto_reconnect && s_retry_count < s_config.retry_config.max_retry_count) {
                    s_retry_count++;
                    ESP_LOGI(TAG, "Retrying connection (%d/%d)", s_retry_count, s_config.retry_config.max_retry_count);
                    vTaskDelay(pdMS_TO_TICKS(s_config.retry_config.retry_interval_ms));
                    esp_wifi_connect();
                } else if (s_retry_count >= s_config.retry_config.max_retry_count) {
                    ESP_LOGW(TAG, "Max retry count reached, entering config mode");
                    wifi_config_clear();
                    wifi_config_mode_start(WIFI_CONFIG_MODE_AP);
                }
                break;

            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "SoftAP started");
                break;

            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "SoftAP stopped");
                break;

            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "Scan done");
                if (s_scanning) {
                    uint16_t ap_count = sizeof(s_scan_results) / sizeof(s_scan_results[0]);
                    esp_wifi_scan_get_ap_records(&ap_count, s_scan_results);
                    s_scan_count = ap_count;
                    s_scan_completed = true;
                    ESP_LOGI(TAG, "Scan results: %d APs found", s_scan_count);
                    s_scanning = false;
                }
                break;

            default:
                /* 其他未处理的事件 */
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        /* 获取到IP地址 */
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        char ip_str[16];
        sprintf(ip_str, IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP address: %s", ip_str);

        s_retry_count = 0;

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

        /* 如果启用了时间同步，启动时间同步 */
        if (s_config.time_sync_config.enabled) {
            wifi_time_sync_enable();
        }
    }
}

/**
 * @brief 初始化STA模式并连接WIFI
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 * 
 * 初始化流程：
 * 1. 设置WIFI模式为STA
 * 2. 加载保存的WIFI配置
 * 3. 配置STA参数（SSID、密码）
 * 4. 设置WIFI配置并启动WIFI
 * 5. 启动连接
 */
static esp_err_t wifi_init_sta(void) {
    /* 设置WIFI模式为STA */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    /* 加载保存的WIFI配置 */
    wifi_saved_config_t wifi_cfg;
    if (wifi_config_load(&wifi_cfg) != ESP_OK || !wifi_cfg.saved) {
        ESP_LOGE(TAG, "No saved WIFI configuration found");
        return ESP_ERR_NOT_FOUND;
    }

    /* 配置STA参数 */
    wifi_config_t sta_config = {
        .sta = {
            .ssid = "",                    /* SSID */
            .password = "",                /* 密码 */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,  /* 认证模式 */
        }
    };
    
    /* 复制SSID和密码 */
    strncpy((char*)sta_config.sta.ssid, wifi_cfg.ssid, sizeof(sta_config.sta.ssid) - 1);
    strncpy((char*)sta_config.sta.password, wifi_cfg.password, sizeof(sta_config.sta.password) - 1);
    
    /* 设置WIFI配置 */
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    
    /* 启动WIFI */
    ESP_ERROR_CHECK(esp_wifi_start());
    
    /* 连接WIFI */
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    ESP_LOGI(TAG, "STA initialization completed, connecting to SSID: %s", wifi_cfg.ssid);
    return ESP_OK;
}

/**
 * @brief 连接到指定的WIFI热点
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_connect(void) {
    s_retry_count = 0;
    return wifi_init_sta();
}

/**
 * @brief 断开WIFI连接
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_disconnect(void) {
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK || mode == WIFI_MODE_NULL) {
        ESP_LOGW(TAG, "WiFi not initialized, skip disconnect");
        return ESP_OK;
    }

    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "WiFi not started yet, skip disconnect");
        return ESP_OK;
    } else if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to disconnect WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WIFI disconnected");
    return ESP_OK;
}

/**
 * @brief 获取WIFI连接状态
 * 
 * @param status 状态结构体指针（输出参数）
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_get_status(wifi_status_t *status) {
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (err == ESP_OK) {
        status->connected = true;
        strncpy(status->ssid, (char*)ap_info.ssid, sizeof(status->ssid) - 1);
        status->rssi = ap_info.rssi;
        status->signal_level = wifi_get_signal_level(ap_info.rssi);
        
        /* 计算连接时长 */
        if (s_connect_start_time > 0) {
            uint32_t elapsed_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS) - s_connect_start_time;
            status->connect_time = elapsed_ms / 1000;
        } else {
            status->connect_time = 0;
        }
        
        /* 获取IP地址 */
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif != NULL && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            snprintf(status->ip_address, sizeof(status->ip_address), IPSTR, IP2STR(&ip_info.ip));
        } else {
            strcpy(status->ip_address, "0.0.0.0");
        }
    } else {
        status->connected = false;
        strcpy(status->ssid, "");
        status->rssi = 0;
        status->signal_level = WIFI_SIGNAL_LEVEL_NONE;
        status->connect_time = 0;
        strcpy(status->ip_address, "0.0.0.0");
    }
    
    return ESP_OK;
}

/**
 * @brief 检查WIFI是否已连接
 * 
 * @return true 已连接
 * @return false 未连接
 */
bool wifi_is_connected(void) {
    wifi_ap_record_t ap_info;
    return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
}

/**
 * @brief 获取信号强度等级
 * 
 * @param rssi 信号强度（dBm）
 * @return wifi_signal_level_t 信号强度等级
 */
wifi_signal_level_t wifi_get_signal_level(int8_t rssi) {
    if (rssi >= -50) {
        return WIFI_SIGNAL_LEVEL_STRONG;
    } else if (rssi >= -60) {
        return WIFI_SIGNAL_LEVEL_MEDIUM;
    } else if (rssi >= -70) {
        return WIFI_SIGNAL_LEVEL_WEAK;
    } else if (rssi >= -80) {
        return WIFI_SIGNAL_LEVEL_VERY_WEAK;
    } else {
        return WIFI_SIGNAL_LEVEL_NONE;
    }
}

/**
 * @brief 扫描可用的WIFI热点
 * 
 * @param ap_list 扫描结果列表（输出参数）
 * @param max_count 最大扫描数量
 * @param count 实际扫描到的数量（输出参数）
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_scan(wifi_ap_record_t *ap_list, uint16_t max_count, uint16_t *count) {
    if (ap_list == NULL || count == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = esp_wifi_scan_start(NULL, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_scan_get_ap_records(&max_count, ap_list);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Get scan records failed: %s", esp_err_to_name(err));
        return err;
    }

    *count = max_count;
    ESP_LOGI(TAG, "Scan completed, found %d APs", *count);

    return ESP_OK;
}

esp_err_t wifi_scan_start_nonblocking(void) {
    if (s_scanning) {
        ESP_LOGW(TAG, "Scan already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    s_scanning = true;
    s_scan_completed = false;
    s_scan_count = 0;

    esp_err_t err = esp_wifi_scan_start(NULL, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(err));
        s_scanning = false;
        return err;
    }

    ESP_LOGI(TAG, "Non-blocking scan started");
    return ESP_OK;
}

bool wifi_scan_is_running(void) {
    return s_scanning;
}

esp_err_t wifi_scan_get_ap_results(wifi_ap_record_t *ap_list, uint16_t max_count, uint16_t *count) {
    if (ap_list == NULL || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_scanning) {
        return ESP_ERR_NOT_FINISHED;
    }

    if (!s_scan_completed) {
        return ESP_ERR_NOT_FOUND;
    }

    uint16_t result_count = s_scan_count;
    if (max_count < result_count) {
        result_count = max_count;
    }

    memcpy(ap_list, s_scan_results, result_count * sizeof(wifi_ap_record_t));
    *count = result_count;

    return ESP_OK;
}

/**
 * @brief 反初始化WIFI管理器
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_manager_deinit(void) {
    if (!s_initialized) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    
    s_initialized = false;
    ESP_LOGI(TAG, "WIFI manager deinitialized");
    return ESP_OK;
}

/**
 * @brief 获取当前WIFI连接状态字符串
 * 
 * @return const char* 状态描述字符串
 */
const char* wifi_get_status_string(void) {
    if (wifi_is_connected()) {
        return "Connected";
    } else {
        return "Disconnected";
    }
}
