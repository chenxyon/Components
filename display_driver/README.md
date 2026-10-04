# Display Driver 组件 v2.2

## 简介

Display Driver 组件提供多屏幕适配的高级显示功能封装，支持 SSD1306 OLED 和 ILI9341 TFT 屏幕。本组件整合了驱动抽象层、LVGL 移植层和统一文本显示接口，提供一站式显示解决方案。

## v2.2 更新内容
- 切换单色屏为 FULL 渲染模式，消除 PARTIAL 模式下的区域对齐和 stride 歧义
- FULL 模式下 LVGL 自动渲染全屏，简化 flush 逻辑
- 修改 flush 函数为一次性连续写入所有页数据（水平地址模式 + 分块传输）
- 修正缓冲区大小计算：FULL 模式使用全屏高度，加上 8 字节 I1 调色板
- LVGL 9.x 内部自动处理 I1 格式 x 坐标对齐（无需外部 rounder 回调）

## v2.1 更新内容
- 修复 LVGL 9.x 兼容性：单色屏从 L8 格式改为 I1 格式（1bit/像素）
- 重写 SSD1306 flush 函数，适配 I1 格式的像素布局（跳过 8 字节调色板）
- 地址模式改为水平地址模式（0x00），支持多页连续写入

## 功能特性

- **多屏幕支持**：SSD1306 OLED (0.91"/0.96")、ILI9341 TFT (2.4")
- **驱动抽象**：通过枚举和注册表实现驱动自动匹配
- **统一输出入口**：自动识别英/中/图标文本类型
- **富文本混合显示**：支持中英文和图标混合显示
- **LVGL 移植层**：支持多屏幕的 LVGL 显示框架
- **事件驱动调度器**：含看门狗的独立任务调度器
- **版本控制**：支持不同项目使用不同版本的组件

## 文件结构

```
display_driver/
├── include/
│   ├── display_driver.h     # 驱动抽象接口
│   ├── display_utils.h      # 统一文本显示接口
│   ├── lvgl_port.h          # LVGL 移植层接口
│   ├── ssd1306_oled.h       # SSD1306 OLED 驱动接口
│   └── ili9341_tft.h        # ILI9341 TFT 驱动接口
├── src/
│   ├── display_registry.c   # 驱动注册表
│   ├── display_utils.c      # 文本工具实现
│   ├── lvgl_port.c          # LVGL 移植层实现
│   ├── ssd1306_oled.c       # SSD1306 OLED 驱动实现
│   └── ili9341_tft.c        # ILI9341 TFT 驱动实现
├── CMakeLists.txt
├── idf_component.yml
└── README.md
```

## 硬件支持

| 屏幕型号 | 分辨率 | 接口 | 颜色深度 |
|----------|--------|------|----------|
| SSD1306 0.91" OLED | 128x32 | I2C | 1-bit |
| SSD1306 0.96" OLED | 128x64 | I2C | 1-bit |
| ILI9341 2.4" TFT | 320x240 | SPI | 16-bit (RGB565) |

## 配置指南

通过 `idf.py menuconfig` 配置显示参数：

### Display Configuration

```
Display Configuration → Display Type
  ├── SSD1306 0.91" OLED (128x32, I2C)
  ├── SSD1306 0.96" OLED (128x64, I2C)
  └── ILI9341 2.4" TFT (320x240, SPI)
```

### I2C Configuration (OLED)

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| I2C SDA GPIO Pin | 16 | I2C 数据引脚 |
| I2C SCL GPIO Pin | 15 | I2C 时钟引脚 |
| I2C Device Address | 0x3C | I2C 设备地址 |
| Enable I2C Scan | n | 是否启用 I2C 扫描 |
| I2C Frequency | 400kHz | 通信频率 |

### SPI Configuration (TFT)

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| SPI MOSI GPIO Pin | 11 | SPI 数据输出 |
| SPI MISO GPIO Pin | -1 | SPI 数据输入 |
| SPI SCLK GPIO Pin | 12 | SPI 时钟 |
| SPI CS GPIO Pin | 10 | 片选 |
| SPI DC GPIO Pin | 9 | 数据/命令控制 |
| SPI RST GPIO Pin | 4 | 复位 |
| SPI Backlight GPIO Pin | -1 | 背光控制 |
| SPI Frequency | 10MHz | 通信频率 |

### Scheduler Configuration

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| Maximum Number of Tasks | 10 | 最大任务数 |
| Watchdog Timeout | 5000ms | 看门狗超时时间 |
| Scheduler Task Priority | 5 | 任务优先级 |
| Scheduler Task Stack Size | 4096 bytes | 任务栈大小 |

## API 参考

### 驱动抽象接口

```c
// 初始化驱动
esp_err_t display_driver_init(display_type_t type, const display_pin_config_t *pins);

// 获取屏幕参数
int display_get_width(void);
int display_get_height(void);
bool display_is_monochrome(void);

// 获取版本
const char *display_driver_get_version(void);
```

### 统一文本显示接口

```c
// 文本类型检测
text_type_t display_detect_text_type(const char *text);

// 创建标签（自动字体识别）
lv_obj_t* display_create_label(lv_obj_t *parent, text_type_t type, const char *text);
lv_obj_t* display_create_label_fmt(lv_obj_t *parent, text_type_t type, const char *format, ...);

// 创建富文本（混合显示）
lv_obj_t* display_create_rich_text(lv_obj_t *parent, const char *text);
lv_obj_t* display_create_rich_text_fmt(lv_obj_t *parent, const char *format, ...);
```

### LVGL 移植层

```c
// 初始化单屏幕
esp_err_t lvgl_port_init(display_type_t type, const display_pin_config_t *pins);

// 初始化多屏幕
esp_err_t lvgl_port_init_multi(display_type_t *types, const display_pin_config_t **pins, int count);

// 启动/停止 LVGL 任务
esp_err_t lvgl_port_start(void);
esp_err_t lvgl_port_stop(void);

// 获取显示对象
lv_display_t *lvgl_port_get_display(void);
lv_display_t *lvgl_port_get_display_by_id(int display_id);

// 获取字体
const lv_font_t *lvgl_port_get_chinese_font(void);
const lv_font_t *lvgl_port_get_font_awesome(void);
const lv_font_t *lvgl_port_get_default_font(void);
```

### 事件驱动调度器

```c
// 初始化调度器
esp_err_t system_scheduler_init(const scheduler_config_t *config);

// 添加任务
esp_err_t system_scheduler_add_task(scheduler_task_cb_t cb, uint32_t interval_ms, const char *name, void *arg);

// 移除任务
esp_err_t system_scheduler_remove_task(scheduler_task_cb_t cb, void *arg);
```

## 使用示例

### 单屏幕初始化

```c
#include "display_driver.h"
#include "lvgl_port.h"

void app_main(void) {
    // 初始化 LVGL 和显示驱动
    esp_err_t ret = lvgl_port_init(DISPLAY_TYPE_TFT_24_ILI9341, NULL);
    if (ret != ESP_OK) return;
    
    // 启动 LVGL 任务
    lvgl_port_start();
    
    // 创建 UI
    lv_obj_t *label = display_create_label(lv_scr_act(), TEXT_TYPE_AUTO, "Hello 你好");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}
```

### 多屏幕初始化

```c
#include "display_driver.h"
#include "lvgl_port.h"

void app_main(void) {
    display_type_t types[2] = {
        DISPLAY_TYPE_TFT_24_ILI9341,
        DISPLAY_TYPE_OLED_091_SSD1306
    };
    
    esp_err_t ret = lvgl_port_init_multi(types, NULL, 2);
    if (ret != ESP_OK) return;
    
    lvgl_port_start();
    
    // 获取主屏
    lv_display_t *tft = lvgl_port_get_display_by_id(0);
    // 获取副屏
    lv_display_t *oled = lvgl_port_get_display_by_id(1);
}
```

### 自动文本类型检测

```c
// 自动检测文本类型，选择对应字体
lv_obj_t *label = display_create_label(lv_scr_act(), TEXT_TYPE_AUTO, "Hello 你好");

// 手动指定文本类型
lv_obj_t *en_label = display_create_label(lv_scr_act(), TEXT_TYPE_ENGLISH, "Hello");
lv_obj_t *cn_label = display_create_label(lv_scr_act(), TEXT_TYPE_CHINESE, "你好");
lv_obj_t *icon_label = display_create_label(lv_scr_act(), TEXT_TYPE_ICON, LV_SYMBOL_WIFI);
```

### 富文本混合显示

```c
// 中英文和图标混合显示
lv_obj_t *rich_text = display_create_rich_text(lv_scr_act(), "WiFi " LV_SYMBOL_WIFI " 已连接");
lv_obj_align(rich_text, LV_ALIGN_CENTER, 0, 0);
```

### 事件驱动调度器

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
    
    // 添加定时任务（每秒执行）
    system_scheduler_add_task(update_clock, 1000, "clock", label);
}
```

## Gitee 私有组件使用指南

### 配置 idf_component.yml

在项目根目录的 `idf_component.yml` 中声明依赖：

```yaml
dependencies:
  display_driver:
    git: git@gitee.com:your-team/display_driver.git
    version: "v2.0.0"
  
  system_scheduler:
    git: git@gitee.com:your-team/system_scheduler.git
    version: "v1.0.0"
```

### 版本切换

```yaml
# 使用 v2.0.0 稳定版
version: "v2.0.0"

# 使用 main 分支（开发版）
version: "main"

# 使用指定 commit
version: "abc123def456"
```

### Gitee SSH 配置

1. 生成 SSH 密钥：
   ```bash
   ssh-keygen -t ed25519 -C "your_email@example.com"
   ```

2. 将公钥添加到 Gitee：
   - 登录 Gitee → 设置 → SSH 公钥
   - 粘贴 `~/.ssh/id_ed25519.pub` 内容

3. 测试连接：
   ```bash
   ssh -T git@gitee.com
   ```

## 迁移指南

### 从 v1.x 迁移到 v2.0

#### 1. 驱动初始化

**旧版：**
```c
custom_lvgl_port_init(128, 32);
```

**新版：**
```c
lvgl_port_init(DISPLAY_TYPE_OLED_091_SSD1306, NULL);
lvgl_port_start();
```

#### 2. 标签创建

**旧版：**
```c
lv_obj_t *label = display_utils_create_label(TEXT_TYPE_ENGLISH, "Hello", LV_ALIGN_TOP_LEFT, 0, 0);
```

**新版：**
```c
lv_obj_t *label = display_create_label(lv_scr_act(), TEXT_TYPE_AUTO, "Hello");
lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
```

#### 3. 文本类型检测

**新版支持自动检测：**
```c
// 自动检测文本类型（英文/中文/图标）
lv_obj_t *label = display_create_label(lv_scr_act(), TEXT_TYPE_AUTO, "Hello 你好");
```

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

### 0.96寸 OLED 屏幕 (SSD1306) - I2C 接口

| ESP32-S3 | OLED 屏幕 | 功能 |
|----------|-----------|------|
| GPIO 16 | SDA | I2C 数据 |
| GPIO 15 | SCL | I2C 时钟 |
| 3.3V | VCC | 电源 |
| GND | GND | 地线 |

## 注意事项

- **字体依赖**：确保字体组件包含 `font_awesome_14_1` 和 `font_puhui_basic_14_1`
- **堆内存**：富文本 `lv_spangroup` 会动态分配，需保证堆空间充足
- **自动检测误判**：某些特殊符号可能被误识别，可提供手动覆盖
- **多屏场景**：`lv_scr_act()` 可能指向非预期屏幕，建议明确指定父对象
- **回调阻塞**：调度器回调中禁止使用阻塞函数（如 `vTaskDelay`）
- **Gitee 权限**：确保所有团队成员都被添加为仓库成员

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v2.2.0 | 2026-08-02 | 切换 FULL 渲染模式、添加 rounder 回调、修复 I1 格式写屏 |
| v2.1.0 | 2026-08-02 | 修复 LVGL 9.x I1 格式兼容性 |
| v2.0.0 | 2026-06-29 | 新增多屏幕支持、统一输出入口、富文本、事件驱动调度器 |
| v1.0.0 | 2026-06-28 | 初始版本，支持 SSD1306 OLED 和基础 LVGL 移植 |

## 许可证

MIT License
