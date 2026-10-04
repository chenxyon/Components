111# ChenYong 组件套件 v2.0

## 简介

ChenYong 组件套件是一组用于 ESP32/ESP32-S3 的嵌入式开发组件，包含显示驱动、UI 工具、版本管理、WIFI 管理、字体和事件调度器等功能模块。所有组件支持版本控制，不同项目可以使用不同版本的组件。

## 组件列表

| 组件 | 说明 | 依赖层级 | 支持芯片 | 版本 |
|------|------|----------|----------|------|
| `version_manager` | 版本号管理 | 第1层 | ESP32/ESP32-S3 | v1.0.0 |
| `system_scheduler` | 事件驱动调度器（含看门狗） | 第1层 | ESP32/ESP32-S3 | v1.0.0 |
| `fonts` | 字体资源（中文、图标） | 第2层 | ESP32/ESP32-S3 | v1.0.0 |
| `ui_utils` | UI 工具函数 | 第2层 | ESP32/ESP32-S3 | v1.0.0 |
| `display_driver` | 多屏幕显示驱动（SSD1306 OLED + ILI9341 TFT + LVGL移植层） | 第3层 | ESP32-S3 | v2.0.0 |
| `wifi_manager` | WIFI 连接管理（NVS持久化、自动连接、配置模式、NTP同步） | 第1层 | ESP32/ESP32-S3/C3/C6 | v1.1.0 |

## 依赖关系图

```
display_driver
    ├── fonts
    ├── ui_utils
    ├── system_scheduler
    └── (esp_lvgl_port, driver, esp_lcd)

wifi_manager
    └── (esp_wifi, esp_event, nvs_flash, lwip, esp_http_server)

system_scheduler
    └── (esp_timer, freertos)

fonts
    └── (lvgl)
```

## 配置菜单

通过 `idf.py menuconfig` 进入 `ChenYong Component Suite` 菜单：

```
ChenYong Component Suite
├── 组件启用配置                    # 启用/禁用各组件
│   ├── Enable Version Manager
│   ├── Enable System Scheduler
│   ├── Enable Fonts
│   ├── Enable UI Utils
│   ├── Enable Display Driver      # 自动启用 fonts, ui_utils, system_scheduler
│   └── Enable WIFI Manager
├── Version Manager                 # 版本管理配置
├── System Scheduler               # 事件调度器配置
├── Fonts Configuration            # 字体配置
├── UI Utils                        # UI 工具配置
├── Display Driver Configuration    # 显示驱动配置
│   ├── Display Type
│   │   ├── SSD1306 0.91" OLED
│   │   ├── SSD1306 0.96" OLED
│   │   ├── ILI9341 2.4" TFT (横屏)
│   │   └── ILI9341 2.4" TFT (竖屏)
│   ├── I2C Configuration (OLED)
│   ├── SPI Configuration (TFT)
│   └── LVGL Configuration
└── WIFI Manager Configuration      # WIFI 管理器配置
    ├── Default Config Mode
    ├── Retry Configuration
    ├── Time Sync Configuration
    └── SoftAP Configuration
```

## 强制依赖

| 组件 | 强制启用的依赖 |
|------|---------------|
| `display_driver` | `fonts`, `ui_utils`, `system_scheduler`, `lvgl`, `esp_lvgl_port`, `driver` |
| `system_scheduler` | `esp_timer`, `freertos` |
| `fonts` | `lvgl` |
| `wifi_manager` | `esp_wifi`, `esp_event`, `nvs_flash`, `lwip`, `esp_http_server` |

## 快速开始

### 1. 项目配置

**方式一：使用 EXTRA_COMPONENT_DIRS（推荐）**

```cmake
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "D:/esp/components/chenyong/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(your_project)
```

**方式二：使用 idf_component.yml**

```yaml
dependencies:
  display_driver:
    path: D:/esp/components/chenyong/components/display_driver
    version: "v2.0.0"
  
  system_scheduler:
    path: D:/esp/components/chenyong/components/system_scheduler
    version: "v1.0.0"
  
  fonts:
    path: D:/esp/components/chenyong/components/fonts
    version: "v1.0.0"
```

### 2. 双屏显示示例

```c
#include "display_driver.h"
#include "lvgl_port.h"
#include "display_utils.h"
#include "lvgl.h"

void app_main(void) {
    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCore(0));
    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCore(1));
    
    display_type_t types[2] = {
        DISPLAY_TYPE_TFT_24_ILI9341,
        DISPLAY_TYPE_OLED_091_SSD1306
    };
    
    esp_err_t ret = lvgl_port_init_multi(types, NULL, 2);
    if (ret != ESP_OK) return;
    
    lvgl_port_start();
    
    // TFT屏幕：显示中英文混合欢迎信息
    lv_display_t *tft = lvgl_port_get_display_by_id(0);
    lv_display_set_default(tft);
    lv_obj_t *tft_label = display_create_rich_text(lv_scr_act(), "欢迎你成功点亮2.4寸屏 - Hello World!!");
    lv_obj_align(tft_label, LV_ALIGN_CENTER, 0, 0);
    
    // OLED屏幕：显示 Hello World!!
    lv_display_t *oled = lvgl_port_get_display_by_id(1);
    lv_display_set_default(oled);
    lv_obj_t *oled_label = display_create_label(lv_scr_act(), TEXT_TYPE_AUTO, "Hello World!!");
    lv_obj_align(oled_label, LV_ALIGN_CENTER, 0, 0);
}
```

### 3. WIFI 管理器示例

```c
#include "wifi_manager.h"

void app_main(void) {
    wifi_manager_config_t config = {
        .retry_config = {
            .max_retry_count = 3,
            .retry_interval_ms = 5000
        },
        .time_sync_config = {
            .enabled = true,
            .sync_interval_min = 60,
            .ntp_server = "pool.ntp.org"
        },
        .auto_reconnect = true,
        .default_config_mode = WIFI_CONFIG_MODE_PHONE,
        .softap_config = {
            .ssid = "ESP32_Config",
            .password = "12345678",
            .channel = 1,
            .hidden = false
        }
    };
    
    wifi_manager_init(&config);
}
```

### 4. 事件驱动调度器示例

```c
#include "system_scheduler.h"

static void update_clock(void *arg) {
    lv_obj_t *label = (lv_obj_t *)arg;
    // 更新时钟显示...
}

void app_main(void) {
    scheduler_config_t cfg = {
        .wdt_timeout_ms = 5000,
        .task_priority = 5,
        .task_stack_size = 4096
    };
    system_scheduler_init(&cfg);
    
    system_scheduler_add_task(update_clock, 1000, "clock", label);
}
```

## 组件详情

各组件详细文档：

- [Display Driver](./components/display_driver/README.md) - 多屏幕显示驱动，支持 SSD1306 OLED 和 ILI9341 TFT
- [System Scheduler](./components/system_scheduler/README.md) - 事件驱动调度器，含看门狗支持
- [Fonts](./components/fonts/README.md) - 字体资源组件
- [UI Utils](./components/ui_utils/README.md) - UI 工具函数
- [Version Manager](./components/version_manager/README.md) - 版本号管理
- [WIFI Manager](./components/wifi_manager/README.md) - WIFI 连接管理

## 支持的屏幕型号

| 屏幕型号 | 分辨率 | 接口 | 颜色深度 | 枚举值 |
|----------|--------|------|----------|--------|
| SSD1306 0.91" OLED | 128x32 | I2C | 1-bit | `DISPLAY_TYPE_OLED_091_SSD1306` |
| SSD1306 0.96" OLED | 128x64 | I2C | 1-bit | `DISPLAY_TYPE_OLED_096_SSD1306` |
| ILI9341 2.4" TFT (横屏) | 320x240 | SPI | 16-bit | `DISPLAY_TYPE_TFT_24_ILI9341` |
| ILI9341 2.4" TFT (竖屏) | 240x320 | SPI | 16-bit | `DISPLAY_TYPE_TFT_24_ILI9341_VERT` |

## 接线对照表

### 2.4寸 TFT 屏幕 (ILI9341) - SPI 接口

| ESP32-S3 | TFT 屏幕 | 功能 |
|----------|----------|------|
| GPIO 11 | SDA | SPI 数据 |
| GPIO 12 | SCK | SPI 时钟 |
| GPIO 10 | CS | 片选 |
| GPIO 9 | DC | 数据/命令 |
| GPIO 4 | RST | 复位 |
| 3.3V | PWR | 电源 |
| GND | GND | 地线 |

### 0.91寸 OLED 屏幕 (SSD1306) - I2C 接口

| ESP32-S3 | OLED 屏幕 | 功能 |
|----------|-----------|------|
| GPIO 16 | SDA | I2C 数据 |
| GPIO 15 | SCL | I2C 时钟 |
| 3.3V | VCC | 电源 |
| GND | GND | 地线 |

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| v2.0.0 | 2026-06-29 | 添加 display_driver v2.0（多屏幕支持、统一输出入口、富文本）、system_scheduler、fonts 组件 |
| v1.1.0 | 2026-06-23 | 添加 WIFI Manager 组件 |
| v1.0.0 | - | 初始版本，包含显示和版本管理组件 |

## 版本管理

本套件采用 **本地 Git 仓库 + path 引用 + 自动标签锁定** 的版本管理方案。详细操作手册请参考：

- [VERSION_MANAGEMENT_GUIDE.md](./VERSION_MANAGEMENT_GUIDE.md)

## 目录结构

```
D:\esp\components\chenyong\
├── README.md                              # 组件套件总文档
├── VERSION_MANAGEMENT_GUIDE.md            # 版本管理操作手册
└── components/                            # 组件目录
    ├── display_driver/                    # 多屏幕显示驱动 v2.0.0
    │   ├── include/
    │   │   ├── display_driver.h           # 驱动抽象接口
    │   │   ├── display_utils.h            # 统一文本显示接口
    │   │   ├── lvgl_port.h                # LVGL 移植层接口
    │   │   ├── ssd1306_oled.h             # SSD1306 OLED 驱动接口
    │   │   └── ili9341_tft.h              # ILI9341 TFT 驱动接口
    │   ├── src/
    │   │   ├── display_registry.c         # 驱动注册表
    │   │   ├── display_utils.c            # 文本工具实现
    │   │   ├── lvgl_port.c                # LVGL 移植层实现
    │   │   ├── ssd1306_oled.c             # SSD1306 OLED 驱动实现
    │   │   └── ili9341_tft.c              # ILI9341 TFT 驱动实现
    │   ├── CMakeLists.txt
    │   ├── idf_component.yml
    │   └── README.md
    ├── system_scheduler/                  # 事件驱动调度器 v1.0.0
    │   ├── include/
    │   │   └── system_scheduler.h         # 调度器接口
    │   ├── src/
    │   │   └── system_scheduler.c         # 调度器实现
    │   ├── CMakeLists.txt
    │   ├── idf_component.yml
    │   └── README.md
    ├── fonts/                             # 字体资源 v1.0.0
    │   ├── include/
    │   │   ├── cbin_font.h                # 中文字体接口
    │   │   └── font_awesome.h             # Font Awesome 图标定义
    │   ├── src/
    │   │   ├── cbin_font.c                # 中文字体数据
    │   │   ├── font_awesome.c             # 图标符号表
    │   │   ├── font_awesome_14_1.c        # Font Awesome 字体数据
    │   │   └── font_puhui_basic_14_1.c    # 中文基础字体数据
    │   ├── CMakeLists.txt
    │   └── README.md
    ├── version_manager/                    # 版本号管理 v1.0.0
    │   ├── include/
    │   │   └── version.h
    │   ├── src/
    │   │   └── version.c
    │   ├── CMakeLists.txt
    │   ├── Kconfig
    │   └── README.md
    ├── ui_utils/                          # UI 工具函数 v1.0.0
    │   ├── include/
    │   │   └── ui_utils.h
    │   ├── src/
    │   │   └── ui_utils.c
    │   ├── CMakeLists.txt
    │   └── README.md
    └── wifi_manager/                      # WIFI 连接管理 v1.1.0
        ├── include/
        │   └── wifi_manager.h
        ├── src/
        │   ├── wifi_manager.c
        │   ├── wifi_config.c
        │   ├── wifi_scan.c
        │   ├── wifi_time_sync.c
        │   ├── wifi_config_mode.c
        │   ├── wifi_softap.c
        │   ├── wifi_webserver.c
        │   └── wifi_screen_ui.c
        ├── CMakeLists.txt
        ├── Kconfig
        ├── README.md
        └── wifi_wiring.md
```

## 许可证

MIT License#   C o m p o n e n t s 
 
 