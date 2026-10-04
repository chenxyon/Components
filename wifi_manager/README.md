# WIFI Manager 组件

## 简介

WIFI Manager 组件是一个基于 ESP-IDF 的完整 WIFI 连接管理解决方案，提供以下核心功能：

- **WIFI配置持久化存储**：使用 NVS（Non-Volatile Storage）存储配置，刷机后不丢失
- **自动连接与重试机制**：支持可配置的重试次数和间隔，失败后自动进入配置模式
- **双配置模式**：手机配置（SoftAP + Web）和屏幕配置（预留接口）
- **NTP时间同步**：WIFI连接成功后自动同步时间，支持定时同步
- **状态查询**：实时查询连接状态、信号强度、连接时长等信息

## 功能特性

### 1. 配置管理

- 使用 NVS 持久化存储 WIFI 配置（SSID、密码）
- 支持保存、加载、清除配置操作
- 配置存在检查，初始化时自动判断连接或配置模式

### 2. 连接管理

- 自动连接已保存的 WIFI 网络
- 可配置最大重试次数（默认3次）
- 可配置重试间隔（默认5秒）
- 支持自动重连功能
- 重试失败后自动进入配置模式

### 3. 扫描功能

- 主动扫描附近可用 WIFI 热点
- 返回 SSID、信号强度（RSSI）、加密方式、信道
- 支持隐藏热点扫描
- 最大返回20个扫描结果
- **AP模式扫描兼容**：纯AP模式下自动切换到APSTA模式进行扫描，扫描完成后恢复原模式

### 4. 配置模式

#### 手机配置模式
- ESP32 创建 SoftAP 热点
- 启动 HTTP Web 服务器（端口80）
- 手机连接热点后通过浏览器访问 `192.168.4.1`
- 网页支持：扫描 WIFI 列表、选择热点、输入密码

#### 屏幕配置模式
- 预留接口，待后续实现
- 支持 OLED/LCD 等显示设备

### 5. 时间同步

- WIFI 连接成功后自动启用 NTP 时间同步
- 使用 `pool.ntp.org` 作为默认 NTP 服务器
- 支持自定义 NTP 服务器地址
- 可配置同步频率（最小单位为分钟）
- 默认时区：北京时间（UTC+8）

### 6. 状态查询

- 连接状态（已连接/未连接）
- 当前连接的 SSID
- 信号强度（RSSI，单位 dBm）
- 信号强度等级（强/中/弱/无）
- 连接时长（秒）
- 分配的 IP 地址

## 依赖关系

### 必需依赖

| 组件 | 版本要求 | 说明 |
|------|----------|------|
| `esp_wifi` | ESP-IDF v5.x | WIFI 驱动 |
| `esp_event` | ESP-IDF v5.x | 事件处理框架 |
| `nvs_flash` | ESP-IDF v5.x | NVS 存储 |
| `lwip` | ESP-IDF v5.x | 网络协议栈 |
| `esp_http_server` | ESP-IDF v5.x | HTTP 服务器 |

### 可选依赖

| 组件 | 说明 |
|------|------|
| `lvgl` | UI 界面库（屏幕配置模式） |

## 使用方法

### 1. 添加组件依赖

在项目的 `CMakeLists.txt` 中添加：

```cmake
REQUIRES wifi_manager
```

### 2. 初始化 WIFI 管理器

```c
#include "wifi_manager.h"

void app_main(void) {
    // 配置参数初始化
    wifi_manager_config_t config = {
        // 重试配置
        .retry_config = {
            .max_retry_count = 3,        // 最大重试次数
            .retry_interval_ms = 5000    // 重试间隔（毫秒）
        },
        // 时间同步配置
        .time_sync_config = {
            .enabled = true,             // 启用时间同步
            .sync_interval_min = 60,     // 同步间隔（分钟）
            .ntp_server = "pool.ntp.org" // NTP服务器地址
        },
        .auto_reconnect = true,          // 启用自动重连
        .default_config_mode = WIFI_CONFIG_MODE_PHONE, // 默认配置模式
        // SoftAP 配置
        .softap_config = {
            .ssid = "ESP32_Config",      // 热点名称
            .password = "12345678",      // 热点密码（为空则开放）
            .channel = 1,                // 信道（1-13）
            .hidden = false              // 是否隐藏热点
        }
    };
    
    // 初始化 WIFI 管理器
    wifi_manager_init(&config);
}
```

### 3. 查询 WIFI 状态

```c
#include "wifi_manager.h"
#include <stdio.h>

void check_wifi_status(void) {
    wifi_status_t status;
    
    if (wifi_get_status(&status) == ESP_OK) {
        if (status.connected) {
            printf("========== WIFI Status ==========\n");
            printf("Connected: Yes\n");
            printf("SSID: %s\n", status.ssid);
            printf("RSSI: %d dBm\n", status.rssi);
            
            // 信号强度等级
            const char *signal_str[] = {"None", "Weak", "Medium", "Strong"};
            printf("Signal Level: %s\n", signal_str[status.signal_level]);
            
            printf("Connect Time: %lu seconds\n", status.connect_time);
            printf("IP Address: %s\n", status.ip_address);
            printf("==================================\n");
        } else {
            printf("WIFI Status: Not connected\n");
        }
    } else {
        printf("Failed to get WIFI status\n");
    }
}
```

### 4. 手动启动配置模式

```c
// 启动手机配置模式（SoftAP + Web）
wifi_config_mode_start(WIFI_CONFIG_MODE_PHONE);

// 启动屏幕配置模式（预留）
// wifi_config_mode_start(WIFI_CONFIG_MODE_SCREEN);
```

### 5. 手机配置流程

1. **设备启动**：如果没有保存的 WIFI 配置，ESP32 会自动创建热点
2. **手机连接**：使用手机连接热点（默认 SSID: `ESP32_Config`，密码: `12345678`）
3. **访问网页**：打开手机浏览器，访问 `http://192.168.4.1`
4. **扫描 WIFI**：点击"扫描 WIFI"按钮，获取附近可用热点列表
5. **选择热点**：点击列表中的热点自动填充 SSID
6. **输入密码**：在密码输入框中输入 WIFI 密码
7. **保存配置**：点击"保存配置"按钮
8. **自动连接**：ESP32 保存配置并自动尝试连接

## 配置选项

通过 `menuconfig` 配置以下选项：

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `WIFI_CONFIG_MODE` | choice | PHONE | 默认配置模式（PHONE/SCREEN） |
| `WIFI_MANAGER_MAX_RETRY_COUNT` | int | 3 | 最大重试次数 |
| `WIFI_MANAGER_RETRY_INTERVAL` | int | 5000 | 重试间隔（毫秒） |
| `WIFI_MANAGER_TIME_SYNC_ENABLED` | bool | y | 启用时间同步 |
| `WIFI_MANAGER_TIME_SYNC_INTERVAL` | int | 60 | 时间同步间隔（分钟） |
| `WIFI_MANAGER_NTP_SERVER` | string | pool.ntp.org | NTP服务器地址 |
| `WIFI_MANAGER_AUTO_RECONNECT` | bool | y | 启用自动重连 |
| `WIFI_SOFTAP_SSID_PREFIX` | string | ESP32_Config | SoftAP SSID前缀 |
| `WIFI_SOFTAP_PASSWORD` | string | 12345678 | SoftAP密码 |
| `WIFI_SOFTAP_CHANNEL` | int | 1 | SoftAP信道 |
| `WIFI_SOFTAP_HIDDEN` | bool | n | 隐藏SoftAP热点 |
| `WIFI_WEBSERVER_PORT` | int | 80 | Web服务器端口 |

## 文件结构

```
wifi_manager/
├── CMakeLists.txt              # 构建配置文件
├── Kconfig                     # menuconfig 配置选项
├── README.md                   # 组件说明文档
├── wifi_wiring.md              # 接线说明文档
├── include/
│   └── wifi_manager.h          # 头文件（API接口定义）
└── src/
    ├── wifi_manager.c          # 主文件（核心逻辑、事件处理）
    ├── wifi_config.c           # 配置管理（NVS存储操作）
    ├── wifi_scan.c             # WIFI扫描功能
    ├── wifi_time_sync.c        # NTP时间同步
    ├── wifi_config_mode.c      # 配置模式管理
    ├── wifi_softap.c           # SoftAP热点功能
    ├── wifi_webserver.c        # HTTP Web服务器
    └── wifi_screen_ui.c        # 屏幕UI（预留接口）
```

## API 参考

### 核心管理

| 函数 | 功能说明 |
|------|----------|
| `wifi_manager_init(config)` | 初始化 WIFI 管理器 |
| `wifi_manager_deinit()` | 反初始化 WIFI 管理器 |

### 配置管理

| 函数 | 功能说明 |
|------|----------|
| `wifi_config_save(config)` | 保存 WIFI 配置到 NVS |
| `wifi_config_load(config)` | 从 NVS 加载 WIFI 配置 |
| `wifi_config_clear()` | 清除 NVS 中的 WIFI 配置 |
| `wifi_config_exists()` | 检查是否有保存的配置 |

### 连接管理

| 函数 | 功能说明 |
|------|----------|
| `wifi_connect()` | 连接 WIFI（使用已保存的配置） |
| `wifi_disconnect()` | 断开当前 WIFI 连接 |
| `wifi_reconnect()` | 重新连接（重置重试计数） |
| `wifi_set_retry_config(config)` | 设置重试配置参数 |

### 扫描功能

| 函数 | 功能说明 |
|------|----------|
| `wifi_scan_start()` | 启动 WIFI 扫描 |
| `wifi_scan_get_results(results, count)` | 获取扫描结果数组 |
| `wifi_scan_free_results(results)` | 释放扫描结果内存 |

### 时间同步

| 函数 | 功能说明 |
|------|----------|
| `wifi_time_sync_enable()` | 启用时间同步 |
| `wifi_time_sync_disable()` | 禁用时间同步 |
| `wifi_time_sync_set_interval(interval_min)` | 设置同步间隔（分钟） |

### 状态查询

| 函数 | 功能说明 |
|------|----------|
| `wifi_get_status(status)` | 获取当前 WIFI 连接状态 |
| `wifi_get_signal_level(rssi)` | 根据 RSSI 计算信号强度等级 |
| `wifi_get_connection_duration()` | 获取连接时长（秒） |

### 配置模式

| 函数 | 功能说明 |
|------|----------|
| `wifi_config_mode_start(mode)` | 启动配置模式 |
| `wifi_config_mode_stop()` | 停止配置模式 |
| `wifi_config_mode_get_type()` | 获取当前配置模式类型 |

### SoftAP

| 函数 | 功能说明 |
|------|----------|
| `wifi_softap_start(config)` | 启动 SoftAP 热点 |
| `wifi_softap_stop()` | 停止 SoftAP 热点 |
| `wifi_softap_get_status(status)` | 获取 SoftAP 状态 |

### Web服务器

| 函数 | 功能说明 |
|------|----------|
| `wifi_webserver_start()` | 启动 HTTP Web 服务器 |
| `wifi_webserver_stop()` | 停止 HTTP Web 服务器 |

### 屏幕UI（预留）

| 函数 | 功能说明 |
|------|----------|
| `wifi_screen_ui_create()` | 创建屏幕 UI |
| `wifi_screen_ui_show()` | 显示屏幕 UI |
| `wifi_screen_ui_hide()` | 隐藏屏幕 UI |
| `wifi_screen_ui_destroy()` | 销毁屏幕 UI |

## 数据结构

### wifi_manager_config_t

```c
typedef struct {
    wifi_retry_config_t retry_config;       // 重试配置
    wifi_time_sync_config_t time_sync_config; // 时间同步配置
    bool auto_reconnect;                   // 是否启用自动重连
    wifi_config_mode_t default_config_mode; // 默认配置模式
    wifi_softap_config_t softap_config;     // SoftAP 配置
} wifi_manager_config_t;
```

### wifi_status_t

```c
typedef struct {
    bool connected;                 // 是否已连接
    char ssid[32];                  // 当前连接的 SSID
    int8_t rssi;                    // 信号强度（dBm）
    wifi_signal_level_t signal_level; // 信号强度等级
    uint32_t connect_time;          // 连接时长（秒）
    char ip_address[16];            // IP 地址
} wifi_status_t;
```

### 信号强度等级

| 等级 | 值 | RSSI范围 | 说明 |
|------|-----|----------|------|
| WIFI_SIGNAL_LEVEL_NONE | 0 | < -80 dBm | 无信号 |
| WIFI_SIGNAL_LEVEL_WEAK | 1 | >= -80 dBm | 弱信号 |
| WIFI_SIGNAL_LEVEL_MEDIUM | 2 | >= -70 dBm | 中等信号 |
| WIFI_SIGNAL_LEVEL_STRONG | 3 | >= -50 dBm | 强信号 |

## 工作流程

```
设备启动
    │
    ▼
检查NVS中是否有保存的WIFI配置
    │
    ├── 有配置 ──► 尝试连接WIFI（重置重试计数器）
    │                  │
    │                  ├── 连接成功 ──► 获取IP地址（重置重试计数器）
    │                  │                  │
    │                  │                  ▼
    │                  │              启动NTP时间同步
    │                  │                  │
    │                  │                  ▼
    │                  │              正常运行
    │                  │
    │                  └── 连接失败 ──► 重试（最多N次，间隔可配置）
    │                                    │
    │                                    ├── 重试成功 ──► 启动NTP时间同步
    │                                    │
    │                                    └── 重试N次失败 ──► 清除配置 + 进入配置模式
    │
    └── 无配置 ──► 进入配置模式
                      │
                      ├── 手机配置模式：启动SoftAP + Web服务器
                      │
                      └── 屏幕配置模式（预留）
```

### 关键流程说明

1. **重试机制**：连接失败后自动重试，最多重试 `max_retry_count` 次，每次间隔 `retry_interval_ms` 毫秒

2. **计数器重置**：
   - 调用 `wifi_connect()` 时重置重试计数器
   - 获取 IP 地址成功时重置重试计数器

3. **配置模式触发条件**：
   - 初始启动无保存配置
   - 连接重试超过最大次数
   - 手动调用 `wifi_config_mode_start()`

4. **扫描兼容性**：当设备处于纯 AP 模式时，`wifi_scan()` 会自动切换到 APSTA 模式完成扫描，确保手机配置时可以正常扫描附近 WIFI

## 注意事项

1. **存储安全**：WIFI 密码存储在 NVS 中，建议对敏感应用进行加密存储
2. **内存管理**：扫描结果由调用者负责释放，使用 `wifi_scan_free_results()`
3. **线程安全**：建议在主线程调用 API，或使用互斥锁保护共享资源
4. **错误处理**：所有 API 都有返回值，调用时需要检查错误码
5. **模式切换**：SoftAP 和 STA 模式不能同时运行，切换时会自动停止当前模式
6. **屏幕配置**：屏幕配置接口已预留，需要根据实际硬件实现

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| v1.1.0 | 2026-06-25 | 新增功能：<br/>- AP模式扫描兼容（自动切换APSTA模式）<br/>- 连接失败超过重试次数自动进入配置模式<br/>- 固件版本检测，刷机后自动清除旧配置<br/>- Web页面支持扫描和选择WIFI热点 |
| v1.0.0 | 2026-06-23 | 初始版本，包含完整的 WIFI 管理功能 |

## 许可证

MIT License
