/**
 * @file wifi_manager.h
 * @brief WIFI管理器组件头文件
 * 
 * 提供WIFI连接、扫描、配置、时间同步等功能的API接口
 * 
 * 功能模块：
 * - 核心管理：初始化、状态查询、事件处理
 * - 连接管理：STA模式连接、自动重连、重试机制
 * - 扫描管理：启动扫描、获取结果
 * - 配置管理：保存/加载配置、配置模式（AP/Web/蓝牙）
 * - 时间同步：SNTP时间同步
 * - SoftAP：热点模式
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

/* ==================== 类型定义 ==================== */

/**
 * @brief WIFI信号强度等级枚举
 */
typedef enum {
    WIFI_SIGNAL_LEVEL_NONE = 0,     /* 无信号 */
    WIFI_SIGNAL_LEVEL_VERY_WEAK,    /* 极弱信号 */
    WIFI_SIGNAL_LEVEL_WEAK,         /* 弱信号 */
    WIFI_SIGNAL_LEVEL_MEDIUM,       /* 中等信号 */
    WIFI_SIGNAL_LEVEL_STRONG,       /* 强信号 */
} wifi_signal_level_t;

/**
 * @brief WIFI保存的配置结构体
 * 
 * 存储WIFI连接所需的配置信息（与ESP-IDF的wifi_config_t区分）：
 * - ssid: WIFI热点名称（最大32字节）
 * - password: WIFI密码（最大64字节）
 * - saved: 是否为已保存的配置
 */
typedef struct {
    char ssid[32];                  /* WIFI热点名称 */
    char password[64];              /* WIFI密码 */
    bool saved;                     /* 是否为已保存的配置 */
} wifi_saved_config_t;

/**
 * @brief WIFI状态结构体
 * 
 * 存储当前WIFI连接状态信息：
 * - connected: 是否已连接
 * - ssid: 当前连接的SSID
 * - rssi: 信号强度（单位：dBm）
 * - signal_level: 信号强度等级
 * - connect_time: 连接时长（单位：秒）
 * - ip_address: 分配的IP地址（点分十进制格式）
 */
typedef struct {
    bool connected;                 /* 是否已连接 */
    char ssid[33];                  /* 当前连接的SSID */
    int8_t rssi;                    /* 信号强度（dBm） */
    wifi_signal_level_t signal_level; /* 信号强度等级 */
    uint32_t connect_time;          /* 连接时长（秒） */
    char ip_address[16];            /* IP地址 */
} wifi_status_t;

/**
 * @brief WIFI扫描结果结构体
 * 
 * 存储单个WIFI热点的扫描结果：
 * - ssid: 热点名称
 * - rssi: 信号强度（dBm）
 * - auth_mode: 加密方式（使用ESP-IDF的wifi_auth_mode_t枚举）
 */
typedef struct {
    char ssid[32];                  /* 热点名称 */
    int8_t rssi;                    /* 信号强度（dBm） */
    wifi_auth_mode_t auth_mode;     /* 加密方式 */
} wifi_scan_result_t;

/**
 * @brief WIFI重试配置结构体
 * 
 * 配置WIFI连接失败后的重试策略：
 * - max_retry_count: 最大重试次数（0表示无限重试）
 * - retry_interval_ms: 重试间隔（单位：毫秒）
 */
typedef struct {
    uint8_t max_retry_count;        /* 最大重试次数 */
    uint32_t retry_interval_ms;     /* 重试间隔（毫秒） */
} wifi_retry_config_t;

/**
 * @brief 时间同步配置结构体
 * 
 * 配置NTP时间同步参数：
 * - enabled: 是否启用时间同步
 * - sync_interval_min: 同步间隔（单位：分钟，最小1分钟）
 * - ntp_server: NTP服务器地址（最大64字节）
 */
typedef struct {
    bool enabled;                   /* 是否启用时间同步 */
    uint32_t sync_interval_min;     /* 同步间隔（分钟） */
    char ntp_server[64];            /* NTP服务器地址 */
} wifi_time_sync_config_t;

/**
 * @brief SoftAP配置结构体
 * 
 * 配置SoftAP热点参数：
 * - ssid: 热点名称（最大32字节）
 * - password: 热点密码（最大64字节，为空则开放）
 * - channel: 信道（1-13）
 * - hidden: 是否隐藏热点
 */
typedef struct {
    char ssid[32];                  /* 热点名称 */
    char password[64];              /* 热点密码 */
    uint8_t channel;                /* 信道 */
    bool hidden;                    /* 是否隐藏 */
} wifi_softap_config_t;

/**
 * @brief WIFI配置模式枚举
 */
typedef enum {
    WIFI_CONFIG_MODE_NONE = 0,      /* 无配置模式 */
    WIFI_CONFIG_MODE_AP,            /* AP配网模式 */
    WIFI_CONFIG_MODE_WEB,           /* Web配网模式 */
    WIFI_CONFIG_MODE_BLE,           /* 蓝牙配网模式 */
} wifi_config_mode_t;

/**
 * @brief WIFI管理器配置结构体
 * 
 * 配置WIFI管理器的全局参数：
 * - retry_config: 重试配置
 * - time_sync_config: 时间同步配置
 * - auto_reconnect: 是否启用自动重连
 * - default_config_mode: 默认配置模式
 * - softap_config: SoftAP配置
 */
typedef struct {
    wifi_retry_config_t retry_config;       /* 重试配置 */
    wifi_time_sync_config_t time_sync_config; /* 时间同步配置 */
    bool auto_reconnect;                   /* 是否启用自动重连 */
    wifi_config_mode_t default_config_mode; /* 默认配置模式 */
    wifi_softap_config_t softap_config;     /* SoftAP配置 */
} wifi_manager_config_t;

/* ==================== 核心管理API ==================== */

/**
 * @brief 初始化WIFI管理器
 * 
 * @param config WIFI管理器配置参数
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_manager_init(const wifi_manager_config_t *config);

/**
 * @brief 反初始化WIFI管理器
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief 获取WIFI连接状态
 * 
 * @param status 状态结构体指针（输出参数）
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_get_status(wifi_status_t *status);

/**
 * @brief 检查WIFI是否已连接
 * 
 * @return true 已连接
 * @return false 未连接
 */
bool wifi_is_connected(void);

/**
 * @brief 获取信号强度等级
 * 
 * @param rssi 信号强度（dBm）
 * @return wifi_signal_level_t 信号强度等级
 */
wifi_signal_level_t wifi_get_signal_level(int8_t rssi);

/* ==================== 连接管理API ==================== */

/**
 * @brief 连接到指定的WIFI热点
 * 
 * @param ssid 热点名称
 * @param password 密码
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_connect(void);

/**
 * @brief 断开WIFI连接
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_disconnect(void);

/**
 * @brief 设置重试配置
 * 
 * @param config 重试配置参数
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_set_retry_config(const wifi_retry_config_t *config);

/* ==================== 扫描API ==================== */

/**
 * @brief 启动WIFI扫描
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_scan_start(void);

/**
 * @brief 获取扫描结果
 * 
 * @param results 扫描结果数组指针（输出参数）
 * @param count 结果数量指针（输出参数）
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_scan_get_results(wifi_scan_result_t **results, uint16_t *count);

/**
 * @brief 释放扫描结果内存
 * 
 * @param results 扫描结果数组指针
 */
void wifi_scan_free_results(wifi_scan_result_t *results);

/* ==================== 时间同步API ==================== */

/**
 * @brief 启用时间同步功能
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_time_sync_enable(void);

/**
 * @brief 禁用时间同步功能
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_time_sync_disable(void);

/**
 * @brief 设置时间同步间隔
 * 
 * @param interval_min 同步间隔（分钟）
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_time_sync_set_interval(uint32_t interval_min);

/* ==================== 状态查询API ==================== */

/**
 * @brief 获取当前WIFI连接状态字符串
 * 
 * @return const char* 状态描述字符串
 */
const char* wifi_get_status_string(void);

/**
 * @brief 扫描可用的WIFI热点
 * 
 * @param ap_list 扫描结果列表（输出参数）
 * @param max_count 最大扫描数量
 * @param count 实际扫描到的数量（输出参数）
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_scan(wifi_ap_record_t *ap_list, uint16_t max_count, uint16_t *count);

/**
 * @brief 启动非阻塞WIFI扫描
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_scan_start_nonblocking(void);

/**
 * @brief 检查扫描是否正在进行中
 * 
 * @return bool true表示扫描中，false表示扫描完成或未开始
 */
bool wifi_scan_is_running(void);

/**
 * @brief 获取非阻塞扫描结果
 * 
 * @param ap_list 扫描结果列表（输出参数）
 * @param max_count 最大扫描数量
 * @param count 实际扫描到的数量（输出参数）
 * @return esp_err_t ESP_OK表示成功，ESP_ERR_NOT_FINISHED表示扫描中
 */
esp_err_t wifi_scan_get_ap_results(wifi_ap_record_t *ap_list, uint16_t max_count, uint16_t *count);

/* ==================== 配置管理API ==================== */

/**
 * @brief 保存WIFI配置到NVS
 * 
 * @param config WIFI配置参数
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_config_save(const wifi_saved_config_t *config);

/**
 * @brief 从NVS加载WIFI配置
 * 
 * @param config WIFI配置结构体指针（输出参数）
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_config_load(wifi_saved_config_t *config);

/**
 * @brief 清除保存的WIFI配置
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_config_clear(void);

/**
 * @brief 启动配置模式
 * 
 * @param mode 配置模式
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_config_mode_start(wifi_config_mode_t mode);

/**
 * @brief 停止配置模式
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_config_mode_stop(void);

/**
 * @brief 获取当前配置模式类型
 * 
 * @return wifi_config_mode_t 当前配置模式类型
 */
wifi_config_mode_t wifi_config_mode_get_current(void);

/* ==================== SoftAP API ==================== */

/**
 * @brief 启动SoftAP模式
 * 
 * @param config SoftAP配置参数
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_manager_softap_start(const wifi_softap_config_t *config);

/**
 * @brief 停止SoftAP模式
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_manager_softap_stop(void);

/* ==================== Web服务器API ==================== */

/**
 * @brief 启动Web配置服务器
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_webserver_start(void);

/**
 * @brief 停止Web配置服务器
 * 
 * @return esp_err_t ESP_OK表示成功，其他表示失败
 */
esp_err_t wifi_webserver_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */
