# UI Utils 组件开发文档

## 概述

UI Utils 是基于 LVGL 的 UI 组件库，提供了桌面系统架构、多输入设备支持、设置 UI、Reset 功能和版本管理等核心能力。

## 架构设计

### 核心层次

```
┌─────────────────────────────────────────────┐
│           UI Manager (桌面管理器)            │
│  - 应用注册/注销/启动                        │
│  - 页面跳转和生命周期管理                    │
├─────────────────────────────────────────────┤
│              Desktop (桌面)                  │
│  - Flex布局自动换行                         │
│  - 应用图标显示                              │
├─────────────────────────────────────────────┤
│           App Item (应用项)                  │
│  - 图标 + 文字组合                          │
│  - 绑定应用ID和点击回调                      │
├─────────────────────────────────────────────┤
│           Input Device (输入设备)            │
│  - 按键驱动 (Keypad)                        │
│  - 触摸板驱动 (Touchpad)                    │
├─────────────────────────────────────────────┤
│            Settings UI (设置UI)              │
│  - 系统信息/网络/蓝牙/音量/亮度/灵敏度        │
├─────────────────────────────────────────────┤
│            Version Check (版本管理)          │
│  - 新版本检测/更新信息展示                   │
└─────────────────────────────────────────────┘
```

## 模块说明

### 1. UI Manager

**头文件**: `include/ui_manager.h`

**图标类型**:

```c
typedef enum {
    ICON_TYPE_NONE = 0,   // 无图标
    ICON_TYPE_IMAGE,      // 图片数据 (lv_img_dsc_t*)
    ICON_TYPE_SYMBOL,     // 字体符号 (char*, 如 LV_SYMBOL_HOME)
} icon_type_t;
```

**核心结构体**:

```c
typedef struct {
    app_id_t id;              // 应用唯一ID
    const char *name;         // 应用名称（显示在图标下）
    icon_type_t icon_type;    // 图标类型
    const void *icon;         // 图标数据（根据类型不同）
    void (*launch_cb)(void);  // 点击图标后的回调函数
    int8_t visible;           // 是否在桌面上显示（-1=未设置，0=隐藏，1=显示）
} app_desc_t;
```

**字段说明**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | `app_id_t` | 应用唯一ID |
| `name` | `const char*` | 应用名称（显示在图标下方） |
| `icon_type` | `icon_type_t` | 图标类型（NONE/IMAGE/SYMBOL） |
| `icon` | `const void*` | 图标数据（根据类型不同） |
| `launch_cb` | `void (*)(void)` | 单击图标后的回调函数 |
| `visible` | `int8_t` | 可见性：-1=未设置（默认显示），0=隐藏，1=显示 |

**图标类型说明**:

| 类型 | icon 字段内容 | 适用场景 |
|------|--------------|----------|
| `ICON_TYPE_NONE` | `NULL` | 不需要图标 |
| `ICON_TYPE_IMAGE` | `lv_img_dsc_t*` 图片描述符 | 需要自定义图形图标 |
| `ICON_TYPE_SYMBOL` | `const char*` 符号字符串 | 使用 LVGL 内置符号，轻量简洁 |

**API 接口**:

| 函数名 | 功能描述 |
|--------|----------|
| `ui_manager_init()` | 初始化 UI 管理器 |
| `ui_manager_register_app(app)` | 注册应用到桌面 |
| `ui_manager_unregister_app(id)` | 从桌面注销应用 |
| `ui_manager_launch_app(id)` | 启动指定应用 |
| `ui_manager_return_to_desktop()` | 返回桌面 |
| `ui_manager_notify_event(event)` | 通知 UI 事件 |
| `ui_manager_is_app_registered(id)` | 检查应用是否已注册 |
| `ui_manager_get_app_visible(id)` | 获取应用在桌面上的可见性状态 |
| `ui_manager_set_app_visible(id, visible)` | 设置应用在桌面上的可见性 |
| `get_current_launching_app_id()` | 获取当前正在启动的应用ID（用于共享回调） |
| `ui_manager_get_layout()` | 获取当前桌面布局 |
| `ui_manager_set_layout(layout)` | 设置桌面布局（使用枚举） |
| `ui_manager_get_current_page()` | 获取当前页码 |
| `ui_manager_get_total_pages()` | 获取总页数 |
| `ui_manager_set_page(page)` | 设置页码 |
| `ui_manager_next_page()` | 下一页 |
| `ui_manager_prev_page()` | 上一页 |

**桌面布局枚举**:

使用枚举定义固定的桌面布局，避免动态缩放计算：

| 枚举值 | 行列数 | 按钮大小 | 图标大小 |
|--------|--------|----------|----------|
| `DESKTOP_LAYOUT_2x2` | 2列 × 2行 | 180 × 200 | 120 |
| `DESKTOP_LAYOUT_2x3` | 2列 × 3行 | 180 × 140 | 100 |
| `DESKTOP_LAYOUT_3x3` | 3列 × 3行 | 120 × 140 | 80 |
| `DESKTOP_LAYOUT_4x2` | 4列 × 2行 | 100 × 200 | 70 |
| `DESKTOP_LAYOUT_4x3` | 4列 × 3行 | 100 × 140 | 65 |

**翻页功能**:

当应用数量超过一屏可显示数量时，自动分页。底部显示页码指示器（如 "1/3"）。

| 交互方式 | 翻页操作 |
|----------|----------|
| **触摸滑动** | 向左滑动 → 下一页；向右滑动 → 上一页 |
| **按键操作** | 键盘导航键翻页 |
| **代码调用** | `ui_manager_next_page()` / `ui_manager_prev_page()` |

**应用可见性控制**:

`app_desc_t` 结构体中的 `visible` 字段（`int8_t` 类型）控制应用图标是否在桌面上显示：

| 值 | 效果 |
|----|------|
| `-1`（默认，未设置） | 自动设为显示（等同于 `true`） |
| `0` 或 `false` | 在桌面上隐藏图标，无法通过输入设备触发 |
| `1` 或 `true` | 在桌面上显示图标 |

> **注意**: `visible` 字段为 `int8_t` 类型，使用 `true`/`false` 时会自动转换为 `1`/`0`。

**注册时控制可见性（推荐方式）**:

在注册应用时直接设置 `visible` 字段，即可控制是否在桌面上显示：

```c
// 注册并显示设置UI
app_desc_t settings_app = {
    .id = 100,
    .name = "Settings",
    .icon_type = ICON_TYPE_SYMBOL,
    .icon = LV_SYMBOL_SETTINGS,
    .launch_cb = settings_launch,
    .visible = true,  // 显示在桌面上
};

// 注册但不显示设置UI
app_desc_t hidden_settings = {
    .id = 100,
    .name = "Settings",
    .icon_type = ICON_TYPE_SYMBOL,
    .icon = LV_SYMBOL_SETTINGS,
    .launch_cb = settings_launch,
    .visible = false,  // 不在桌面上显示
};
```

**其他控制方式**:

| 方式 | 作用范围 | 方法 |
|------|----------|------|
| **编译时** | 全局生效 | 设置 `ENABLE_SETTINGS_UI=0` 宏 |
| **运行时** | 动态切换 | 调用 `ui_manager_set_app_visible(id, false)` |

**单击事件绑定机制**:

通过 `app_desc_t` 结构体中的 `launch_cb` 字段绑定单击图标后要执行的代码。

**工作流程**:
1. 注册应用时，通过 `launch_cb` 字段传入回调函数指针
2. 创建图标按钮时，将应用ID存储到按钮的 `user_data`，并绑定统一的点击回调
3. 用户单击图标时，通过 `user_data` 获取应用ID，查找并执行对应的 `launch_cb`

**绑定方法**:

| 方法 | 适用场景 | 说明 |
|------|----------|------|
| **静态函数** | 功能复杂、需要复用 | 推荐使用 |
| **共享回调+上下文** | 多个相似应用 | 通过外部数组传递参数 |

### 2. Input Device

**头文件**: `include/input_device.h`

**按键映射**:

```c
#define KEY_UP      LV_KEY_UP
#define KEY_DOWN    LV_KEY_DOWN
#define KEY_LEFT    LV_KEY_LEFT
#define KEY_RIGHT   LV_KEY_RIGHT
#define KEY_ENTER   LV_KEY_ENTER
```

**输入事件类型**:

```c
typedef enum {
    INPUT_EVENT_SINGLE_CLICK,  // 单击事件
    INPUT_EVENT_DOUBLE_CLICK,  // 双击事件
    INPUT_EVENT_KEY_PRESS,     // 按键按下
    INPUT_EVENT_KEY_RELEASE,   // 按键释放
} input_event_type_t;
```

**API 接口**:

| 函数名 | 功能描述 |
|--------|----------|
| `keypad_init(cb)` | 初始化按键驱动 |
| `keypad_deinit()` | 反初始化按键驱动 |
| `keypad_scan()` | 扫描按键状态 |
| `touchpad_init(cb)` | 初始化触摸板驱动 |
| `touchpad_deinit()` | 反初始化触摸板驱动 |
| `touchpad_scan()` | 扫描触摸板状态 |
| `input_device_init()` | 初始化所有输入设备 |
| `input_device_deinit()` | 反初始化所有输入设备 |

### 3. Settings UI

**头文件**: `include/settings_ui.h`

**菜单图标**:

设置菜单中的每个选项都配有LVGL内置图标：

| 菜单项 | 图标常量 | 说明 |
|--------|----------|------|
| System Info | `LV_SYMBOL_INFO` | 信息图标 |
| Network | `LV_SYMBOL_WIFI` | WiFi图标 |
| Bluetooth | `LV_SYMBOL_BLUETOOTH` | 蓝牙图标 |
| Sound | `LV_SYMBOL_AUDIO` | 音频图标 |
| Display | `LV_SYMBOL_SCREEN` | 屏幕图标 |
| Keyboard | `LV_SYMBOL_KEYBOARD` | 键盘图标 |
| Desktop | `LV_SYMBOL_HOME` | 主页图标 |
| Reset to Defaults | `LV_SYMBOL_REFRESH` | 刷新图标 |

**设置数据结构体**:

```c
typedef struct {
    char project_version[32];     // 项目版本
    char component_version[32];   // 组件版本
    char ip_address[16];          // IP地址
    char ssid[32];                // WiFi名称
    int wifi_signal_level;        // WiFi信号强度
    bool wifi_connected;          // WiFi连接状态
    bool bluetooth_enabled;       // 蓝牙状态
    char bluetooth_device_name[32];// 蓝牙设备名
    int volume;                   // 音量 (0-100)
    int brightness;               // 亮度 (0-100)
    int key_sensitivity;          // 按键灵敏度 (0-100)
} settings_data_t;
```

**API 接口**:

| 函数名 | 功能描述 |
|--------|----------|
| `settings_ui_init()` | 初始化设置 UI |
| `settings_ui_deinit()` | 反初始化设置 UI |
| `settings_ui_show()` | 显示设置 UI |
| `settings_ui_hide()` | 隐藏设置 UI |
| `settings_ui_set_version(proj, comp)` | 设置版本信息 |
| `settings_ui_update_wifi_status(ssid, ip, sig, conn)` | 更新 WiFi 状态 |
| `settings_ui_update_bluetooth_status(enabled, name)` | 更新蓝牙状态 |
| `settings_ui_get_volume()` | 获取当前音量 |
| `settings_ui_get_brightness()` | 获取当前亮度 |
| `settings_ui_get_key_sensitivity()` | 获取按键灵敏度 |
| `settings_ui_set_volume(vol)` | 设置音量 |
| `settings_ui_set_brightness(bright)` | 设置亮度 |
| `settings_ui_set_key_sensitivity(sens)` | 设置按键灵敏度 |
| `settings_ui_reset_to_defaults()` | 重置所有设置为默认值 |

**还原默认设置**:

调用 `settings_ui_reset_to_defaults()` 会将以下设置项恢复为默认值：

| 设置项 | 默认值 |
|--------|--------|
| Volume（音量） | 50 |
| Brightness（亮度） | 80 |
| Key Sensitivity（按键灵敏度） | 50 |
| Icons per Row（每行图标数） | 4 |
| WiFi Status（WiFi状态） | Not connected |
| Bluetooth（蓝牙） | Disabled |

### 4. UI Reset

**头文件**: `include/ui_reset.h`

**API 接口**:

| 函数名 | 功能描述 |
|--------|----------|
| `ui_reset_init(cb)` | 初始化 Reset 功能 |
| `ui_reset_deinit()` | 反初始化 Reset 功能 |
| `ui_reset_check()` | 检查是否触发 Reset |
| `ui_reset_is_triggered()` | 查询是否已触发 Reset |

**触发条件**: 同时按下指定组合键并维持 3 秒以上

### 5. Version Check

**头文件**: `include/version_check.h`

**版本信息结构体**:

```c
typedef struct {
    char current_version[MAX_VERSION_LEN];  // 当前版本
    char latest_version[MAX_VERSION_LEN];   // 最新版本
    char update_log[MAX_UPDATE_LOG_LEN];    // 更新日志
    char package_size[MAX_PACKAGE_SIZE_LEN];// 包大小
    bool update_available;                  // 是否有更新
    bool check_in_progress;                 // 是否正在检查
} version_info_t;
```

**API 接口**:

| 函数名 | 功能描述 |
|--------|----------|
| `version_check_init()` | 初始化版本检查 |
| `version_check_deinit()` | 反初始化版本检查 |
| `version_check_start()` | 开始检查新版本 |
| `version_check_stop()` | 停止检查 |
| `version_check_get_info()` | 获取版本信息 |
| `version_check_has_update()` | 是否有可用更新 |

## 使用示例

### 示例 1: 使用字体符号作为图标

```c
#include "ui_manager.h"
#include "input_device.h"

static void settings_launch(void) {
    // 创建设置界面
}

static void wifi_launch(void) {
    // 创建WiFi界面
}

void ui_system_init(void) {
    ui_manager_init();
    
    app_desc_t settings_app = {
        .id = 1,
        .name = "Settings",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_SETTINGS,
        .launch_cb = settings_launch,
    };
    ui_manager_register_app(&settings_app);
    
    app_desc_t wifi_app = {
        .id = 2,
        .name = "WiFi",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_WIFI,
        .launch_cb = wifi_launch,
    };
    ui_manager_register_app(&wifi_app);
}
```

### 示例 2: 使用图片作为图标

```c
#include "ui_manager.h"

LV_IMG_DECLARE(my_icon_img);

static void my_app_launch(void) {
    // 创建应用界面
}

void register_my_app(void) {
    app_desc_t my_app = {
        .id = 3,
        .name = "My App",
        .icon_type = ICON_TYPE_IMAGE,
        .icon = &my_icon_img,
        .launch_cb = my_app_launch,
    };
    ui_manager_register_app(&my_app);
}
```

### 示例 3: 单击事件绑定 - 静态函数（推荐）

```c
#include "ui_manager.h"
#include "settings_ui.h"

static void settings_launch(void) {
    settings_ui_show();
}

static void back_btn_click_cb(lv_event_t *e) {
    (void)e;
    ui_manager_return_to_desktop();
}

static void custom_launch(void) {
    lv_obj_clean(lv_scr_act());
    
    lv_obj_t *page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(page, LV_HOR_RES_MAX, LV_VER_RES_MAX);
    
    lv_obj_t *title = lv_label_create(page);
    lv_label_set_text(title, "Custom App");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    lv_obj_t *back_btn = lv_btn_create(page);
    lv_obj_set_size(back_btn, 120, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Return");
    lv_obj_center(back_label);
    
    lv_obj_add_event_cb(back_btn, back_btn_click_cb, LV_EVENT_CLICKED, NULL);
}

void register_apps_with_click(void) {
    app_desc_t settings_app = {
        .id = 1,
        .name = "Settings",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_SETTINGS,
        .launch_cb = settings_launch,
    };
    ui_manager_register_app(&settings_app);
    
    app_desc_t custom_app = {
        .id = 2,
        .name = "My App",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_HOME,
        .launch_cb = custom_launch,
    };
    ui_manager_register_app(&custom_app);
}
```

### 示例 4: 单击事件绑定 - 共享回调+上下文

```c
#include "ui_manager.h"

typedef struct {
    int app_type;
    const char *title;
} app_context_t;

static app_context_t g_app_ctx[MAX_APP_COUNT];

static void back_btn_click_cb(lv_event_t *e) {
    (void)e;
    ui_manager_return_to_desktop();
}

static void generic_launch(void) {
    app_id_t current_id = get_current_launching_app_id();
    app_context_t *ctx = &g_app_ctx[current_id];
    
    lv_obj_clean(lv_scr_act());
    
    lv_obj_t *page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(page, LV_HOR_RES_MAX, LV_VER_RES_MAX);
    
    lv_obj_t *label = lv_label_create(page);
    lv_label_set_text_fmt(label, "App Type: %d\nTitle: %s", ctx->app_type, ctx->title);
    lv_obj_center(label);
    
    lv_obj_t *back_btn = lv_btn_create(page);
    lv_obj_set_size(back_btn, 100, 40);
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);
    
    lv_obj_add_event_cb(back_btn, back_btn_click_cb, LV_EVENT_CLICKED, NULL);
}

void register_similar_apps(void) {
    g_app_ctx[1] = (app_context_t){.app_type = 1, .title = "Settings"};
    app_desc_t app1 = {
        .id = 1,
        .name = "Settings",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_SETTINGS,
        .launch_cb = generic_launch,
    };
    ui_manager_register_app(&app1);
    
    g_app_ctx[2] = (app_context_t){.app_type = 2, .title = "WiFi"};
    app_desc_t app2 = {
        .id = 2,
        .name = "WiFi",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_WIFI,
        .launch_cb = generic_launch,
    };
    ui_manager_register_app(&app2);
    
    g_app_ctx[3] = (app_context_t){.app_type = 3, .title = "Bluetooth"};
    app_desc_t app3 = {
        .id = 3,
        .name = "Bluetooth",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_BLUETOOTH,
        .launch_cb = generic_launch,
    };
    ui_manager_register_app(&app3);
}
```

### 示例 5: 初始化完整系统（含设置UI可见性控制）

```c
#include "ui_manager.h"
#include "input_device.h"
#include "settings_ui.h"
#include "ui_reset.h"
#include "version_check.h"

#define ENABLE_SETTINGS_ICON 1

static void app1_launch(void) {
    // 创建应用1界面
}

static void app2_launch(void) {
    // 创建应用2界面
}

static void settings_launch(void) {
    settings_ui_show();
}

void ui_system_init(void) {
    ui_manager_init();
    
    app_desc_t app1 = {
        .id = 1,
        .name = "App 1",
        .icon_type = ICON_TYPE_NONE,
        .icon = NULL,
        .launch_cb = app1_launch,
        .visible = true,
    };
    ui_manager_register_app(&app1);
    
    app_desc_t app2 = {
        .id = 2,
        .name = "App 2",
        .icon_type = ICON_TYPE_NONE,
        .icon = NULL,
        .launch_cb = app2_launch,
        .visible = true,
    };
    ui_manager_register_app(&app2);
    
    app_desc_t settings_app = {
        .id = 100,
        .name = "Settings",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_SETTINGS,
        .launch_cb = settings_launch,
        .visible = ENABLE_SETTINGS_ICON,  // 注册时定义是否显示设置UI
    };
    ui_manager_register_app(&settings_app);
    
    input_device_init();
    settings_ui_init();
    ui_reset_init(NULL);
    version_check_init();
}
```

### 示例 6: 更新 WiFi 和蓝牙状态

```c
void update_network_status(void) {
    settings_ui_update_wifi_status("MyWiFi", "192.168.1.100", 3, true);
    settings_ui_update_bluetooth_status(true, "MyDevice");
}
```

### 示例 7: 调整系统参数

```c
void adjust_system_params(void) {
    settings_ui_set_volume(75);
    settings_ui_set_brightness(80);
    settings_ui_set_key_sensitivity(60);
}
```

### 示例 8: 还原默认设置

```c
#include "settings_ui.h"

void restore_to_defaults(void) {
    settings_ui_reset_to_defaults();
}

void reset_on_button_press(lv_event_t *e) {
    (void)e;
    restore_to_defaults();
}
```

### 示例 9: 版本检查

```c
void check_for_updates(void) {
    version_check_start();
    
    const version_info_t *info = version_check_get_info();
    if (info->update_available) {
        printf("New version available: %s\n", info->latest_version);
        printf("Update log:\n%s\n", info->update_log);
        printf("Package size: %s\n", info->package_size);
    }
}
```

### 示例 9: Reset 功能回调

```c
static void on_reset_triggered(void) {
    printf("Reset triggered! All settings restored to defaults.\n");
    ui_manager_return_to_desktop();
}

void init_reset(void) {
    ui_reset_init(on_reset_triggered);
}

void main_loop(void) {
    while (1) {
        ui_reset_check();
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### 示例 10: 运行时隐藏/显示设置UI

```c
#include "ui_manager.h"

#define SETTINGS_APP_ID 100

void hide_settings_ui(void) {
    ui_manager_set_app_visible(SETTINGS_APP_ID, false);
}

void show_settings_ui(void) {
    ui_manager_set_app_visible(SETTINGS_APP_ID, true);
}

bool is_settings_ui_visible(void) {
    return ui_manager_get_app_visible(SETTINGS_APP_ID);
}

void toggle_settings_ui(void) {
    if (ui_manager_is_app_registered(SETTINGS_APP_ID)) {
        bool visible = ui_manager_get_app_visible(SETTINGS_APP_ID);
        ui_manager_set_app_visible(SETTINGS_APP_ID, !visible);
    }
}
```

### 示例 11: App Item 显示与不显示控制

```c
#include "ui_manager.h"

#define WIFI_APP_ID      1
#define BLUETOOTH_APP_ID 2
#define MUSIC_APP_ID     3
#define GAME_APP_ID      4

static void wifi_launch(void) {}
static void bluetooth_launch(void) {}
static void music_launch(void) {}
static void game_launch(void) {}

void register_all_apps(void) {
    app_desc_t wifi_app = {
        .id = WIFI_APP_ID,
        .name = "WiFi",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_WIFI,
        .launch_cb = wifi_launch,
        .visible = true,  // 显示在桌面
    };
    ui_manager_register_app(&wifi_app);
    
    app_desc_t bluetooth_app = {
        .id = BLUETOOTH_APP_ID,
        .name = "Bluetooth",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_BLUETOOTH,
        .launch_cb = bluetooth_launch,
        .visible = true,  // 显示在桌面
    };
    ui_manager_register_app(&bluetooth_app);
    
    app_desc_t music_app = {
        .id = MUSIC_APP_ID,
        .name = "Music",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_AUDIO,
        .launch_cb = music_launch,
        .visible = false,  // 不在桌面显示（注册但隐藏）
    };
    ui_manager_register_app(&music_app);
    
    app_desc_t game_app = {
        .id = GAME_APP_ID,
        .name = "Game",
        .icon_type = ICON_TYPE_SYMBOL,
        .icon = LV_SYMBOL_GAMEPAD,
        .launch_cb = game_launch,
        .visible = true,  // 显示在桌面
    };
    ui_manager_register_app(&game_app);
}

void control_app_visibility(app_id_t app_id, bool visible) {
    if (ui_manager_is_app_registered(app_id)) {
        ui_manager_set_app_visible(app_id, visible);
    }
}

void toggle_all_apps_visible(void) {
    control_app_visibility(WIFI_APP_ID, !ui_manager_get_app_visible(WIFI_APP_ID));
    control_app_visibility(BLUETOOTH_APP_ID, !ui_manager_get_app_visible(BLUETOOTH_APP_ID));
    control_app_visibility(MUSIC_APP_ID, !ui_manager_get_app_visible(MUSIC_APP_ID));
    control_app_visibility(GAME_APP_ID, !ui_manager_get_app_visible(GAME_APP_ID));
}
```

### 示例 12: 设置桌面布局

```c
#include "ui_manager.h"

void set_desktop_layout_2x2(void) {
    ui_manager_set_layout(DESKTOP_LAYOUT_2x2);
}

void set_desktop_layout_3x3(void) {
    ui_manager_set_layout(DESKTOP_LAYOUT_3x3);
}

void adjust_layout_based_on_screen(int screen_width, int screen_height) {
    if (screen_width >= 800 && screen_height >= 600) {
        ui_manager_set_layout(DESKTOP_LAYOUT_4x3);
    } else if (screen_width >= 480 && screen_height >= 480) {
        ui_manager_set_layout(DESKTOP_LAYOUT_3x3);
    } else if (screen_width >= 480) {
        ui_manager_set_layout(DESKTOP_LAYOUT_4x2);
    } else {
        ui_manager_set_layout(DESKTOP_LAYOUT_2x2);
    }
}

desktop_layout_t get_current_layout(void) {
    return ui_manager_get_layout();
}
```

### 示例 13: 翻页操作

```c
#include "ui_manager.h"

void go_to_next_page(void) {
    ui_manager_next_page();
}

void go_to_prev_page(void) {
    ui_manager_prev_page();
}

void go_to_page(int page) {
    ui_manager_set_page(page);
}

void get_page_info(int *current, int *total) {
    *current = ui_manager_get_current_page();
    *total = ui_manager_get_total_pages();
}

void check_and_go_to_first_page(void) {
    int total = ui_manager_get_total_pages();
    if (total > 0) {
        ui_manager_set_page(0);
    }
}
```

## 编译配置

**CMakeLists.txt**:

```cmake
idf_component_register(
    SRCS "src/ui_utils.c"
         "src/ui_manager.c"
         "src/input_device.c"
         "src/settings_ui.c"
         "src/ui_reset.c"
         "src/version_check.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl__lvgl
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ENABLE_KEYPAD_INPUT=1
    ENABLE_TOUCHPAD_INPUT=1
    ENABLE_SETTINGS_UI=1
)
```

**配置宏说明**:

| 宏名 | 功能 | 默认值 |
|------|------|--------|
| `ENABLE_KEYPAD_INPUT` | 启用按键输入 | 1 |
| `ENABLE_TOUCHPAD_INPUT` | 启用触摸板输入 | 1 |
| `ENABLE_SETTINGS_UI` | 启用设置 UI | 1 |
| `MAX_APP_COUNT` | 最大应用数量 | 10 |
| `DOUBLE_CLICK_TIMEOUT_MS` | 双击检测超时 | 300 |
| `RESET_COMBINE_HOLD_MS` | Reset 长按时间 | 3000 |

## 文件结构

```
ui_utils/
├── include/
│   ├── ui_utils.h          # 基础 UI 工具函数
│   ├── ui_manager.h        # 桌面管理器
│   ├── input_device.h      # 输入设备驱动
│   ├── settings_ui.h       # 设置 UI
│   ├── ui_reset.h          # Reset 功能
│   └── version_check.h     # 版本管理
├── src/
│   ├── ui_utils.c          # 基础工具实现
│   ├── ui_manager.c        # 桌面管理器实现
│   ├── input_device.c      # 输入设备实现
│   ├── settings_ui.c       # 设置 UI 实现
│   ├── ui_reset.c          # Reset 实现
│   └── version_check.c     # 版本管理实现
└── CMakeLists.txt          # 编译配置
```

## 扩展指南

### 添加新应用

1. 定义应用描述结构体
2. 实现启动回调函数
3. 调用 `ui_manager_register_app()` 注册

### 添加新设置项

1. 在 `settings_data_t` 中添加字段
2. 在 `settings_ui.c` 中添加对应的 UI 控件和回调
3. 更新 `settings_ui_reset_to_defaults()` 函数

### 添加新输入设备

1. 在 `input_device.c` 中添加设备初始化和扫描逻辑
2. 在 `input_device_init()` 中注册设备
3. 配置对应的编译宏

## 注意事项

1. 所有 UI 操作必须在 LVGL 任务中执行
2. 应用图标数据需提前准备好
3. Reset 功能需要硬件按键支持
4. 版本检查需要网络访问能力（实际项目中需实现）
