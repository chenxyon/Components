# Fonts 组件 v1.0

## 简介

Fonts 组件提供 ESP32 嵌入式开发所需的字体资源，包含中文基础字体、Font Awesome 图标字体等。所有字体均已转换为 LVGL 可用的格式，可直接用于显示驱动组件。

## 功能特性

- **中文字体**：包含常用中文基础字库（14x14 点阵）
- **图标字体**：Font Awesome 图标库（14x14 点阵），支持 100+ 常用图标
- **ASCII 字体**：标准 ASCII 字符集支持
- **LVGL 兼容**：所有字体均为 LVGL font 格式，可直接用于 LVGL 显示

## 文件结构

```
fonts/
├── include/
│   ├── cbin_font.h          # 中文字体接口
│   └── font_awesome.h       # Font Awesome 图标定义
├── src/
│   ├── cbin_font.c          # 中文字体数据
│   ├── font_awesome.c       # 图标符号表
│   ├── font_awesome_14_1.c  # Font Awesome 14x14 字体数据
│   └── font_puhui_basic_14_1.c  # 中文基础字体数据
├── CMakeLists.txt
└── README.md
```

## 支持的字体

| 字体名称 | 尺寸 | 字符集 | 文件 |
|----------|------|--------|------|
| Font Awesome | 14x14 | 100+ 图标 | font_awesome_14_1.c |
| 中文基础字体 | 14x14 | 常用中文 | font_puhui_basic_14_1.c |

## API 参考

### Font Awesome 图标

```c
#include "font_awesome.h"

// 预定义图标宏
#define FONT_AWESOME_WIFI "\xef\x87\xab"
#define FONT_AWESOME_BATTERY_FULL "\xef\x89\x80"
#define FONT_AWESOME_HEART "\xef\x80\x84"
#define FONT_AWESOME_STAR "\xef\x80\x85"
#define FONT_AWESOME_CHECK "\xef\x80\x8c"
#define FONT_AWESOME_XMARK "\xef\x80\x8d"
#define FONT_AWESOME_GEAR "\xef\x80\x93"
#define FONT_AWESOME_LOCK "\xef\x80\xa3"
#define FONT_AWESOME_UNLOCK "\xef\x82\x9c"
#define FONT_AWESOME_CLOCK "\xef\x80\x97"
#define FONT_AWESOME_TEMPERATURE_HALF "\xef\x8b\x89"
#define FONT_AWESOME_SUN "\xef\x86\x85"
#define FONT_AWESOME_MOON "\xef\x86\x86"
#define FONT_AWESOME_CLOUD "\xef\x83\x82"
#define FONT_AWESOME_DOWNLOAD "\xef\x80\x99"
#define FONT_AWESOME_USER "\xef\x80\x87"
#define FONT_AWESOME_BELL "\xef\x83\xb3"
#define FONT_AWESOME_LOCATION_DOT "\xef\x8f\x85"
#define FONT_AWESOME_GLOBE "\xef\x82\xac"
#define FONT_AWESOME_CAMERA "\xef\x80\xb0"
#define FONT_AWESOME_CALENDAR "\xef\x84\xb3"
#define FONT_AWESOME_ENVELOPE "\xef\x83\xa0"
#define FONT_AWESOME_PHONE "\xef\x82\x95"
#define FONT_AWESOME_COMPASS "\xef\x85\x8e"
#define FONT_AWESOME_CALCULATOR "\xef\x87\xac"
#define FONT_AWESOME_MAGNIFYING_GLASS "\xef\x80\x82"
#define FONT_AWESOME_PLAY "\xef\x81\x8b"
#define FONT_AWESOME_PAUSE "\xef\x81\x8c"
#define FONT_AWESOME_STOP "\xef\x81\x8d"
#define FONT_AWESOME_ARROW_LEFT "\xef\x81\xa0"
#define FONT_AWESOME_ARROW_RIGHT "\xef\x81\xa1"
#define FONT_AWESOME_ARROW_UP "\xef\x81\xa2"
#define FONT_AWESOME_ARROW_DOWN "\xef\x81\xa3"
#define FONT_AWESOME_VOLUME_HIGH "\xef\x80\xa8"
#define FONT_AWESOME_VOLUME_LOW "\xef\x80\xa7"
#define FONT_AWESOME_VOLUME_XMARK "\xef\x9a\xa9"
#define FONT_AWESOME_MUSIC "\xef\x80\x81"
#define FONT_AWESOME_TRASH "\xef\x87\xb8"
#define FONT_AWESOME_HOUSE "\xef\x80\x95"
#define FONT_AWESOME_IMAGE "\xef\x80\xbe"
#define FONT_AWESOME_PEN_TO_SQUARE "\xef\x81\x84"
#define FONT_AWESOME_POWER_OFF "\xef\x80\x91"
#define FONT_AWESOME_SIGNAL "\xef\x80\x92"
#define FONT_AWESOME_BLUETOOTH "\xef\x8a\x93"
#define FONT_AWESOME_COMMENT "\xef\x81\xb5"
#define FONT_AWESOME_LINK "\xef\x83\x81"
#define FONT_AWESOME_CIRCLE_INFO "\xef\x81\x9a"
#define FONT_AWESOME_CIRCLE_QUESTION "\xef\x81\x99"
#define FONT_AWESOME_CIRCLE_CHECK "\xef\x81\x98"
#define FONT_AWESOME_CIRCLE_XMARK "\xef\x81\x97"
#define FONT_AWESOME_ALARM_CLOCK "\xef\x8d\x8e"
#define FONT_AWESOME_SPINNER "\xef\x84\x90"
#define FONT_AWESOME_HEADPHONES "\xef\x80\xa5"
#define FONT_AWESOME_MICROPHONE "\xef\x84\xb0"
#define FONT_AWESOME_MICROPHONE_SLASH "\xef\x84\xb1"
#define FONT_AWESOME_COMMENT_QUESTION "\xee\x85\x8b"
#define FONT_AWESOME_BRIGHTNESS "\xee\x83\x89"
#define FONT_AWESOME_GLASSES "\xef\x94\xb0"
#define FONT_AWESOME_HEART "\xef\x80\x84"
#define FONT_AWESOME_STAR "\xef\x80\x85"
#define FONT_AWESOME_GAMEPAD "\xef\x84\x9b"
#define FONT_AWESOME_WATCH "\xef\x8b\xa1"
#define FONT_AWESOME_ARROWS_REPEAT "\xef\x8d\xa4"
#define FONT_AWESOME_ARROWS_ROTATE "\xef\x80\xa1"
#define FONT_AWESOME_ANGLE_LEFT "\xef\x84\x84"
#define FONT_AWESOME_ANGLE_RIGHT "\xef\x84\x85"
#define FONT_AWESOME_ANGLE_UP "\xef\x84\x86"
#define FONT_AWESOME_ANGLE_DOWN "\xef\x84\x87"
#define FONT_AWESOME_ANGLES_LEFT "\xef\x84\x80"
#define FONT_AWESOME_ANGLES_RIGHT "\xef\x84\x81"
#define FONT_AWESOME_ANGLES_UP "\xef\x84\x82"
#define FONT_AWESOME_ANGLES_DOWN "\xef\x84\x83"
#define FONT_AWESOME_CLOUD_ARROW_DOWN "\xef\x83\xad"
#define FONT_AWESOME_CLOUD_ARROW_UP "\xef\x83\xae"
#define FONT_AWESOME_CLOUD_SLASH "\xee\x84\xb7"
#define FONT_AWESOME_CLOUDS "\xef\x9d\x84"
#define FONT_AWESOME_CLOUD_SUN "\xef\x9d\x86"
#define FONT_AWESOME_CLOUD_SUN_RAIN "\xef\x9d\x83"
#define FONT_AWESOME_CLOUD_MOON "\xef\x9b\x83"
#define FONT_AWESOME_CLOUD_BOLT "\xef\x9d\xac"
#define FONT_AWESOME_CLOUD_HAIL "\xef\x9c\xba"
#define FONT_AWESOME_CLOUD_SLEET "\xef\x9d\x81"
#define FONT_AWESOME_CLOUD_DRIZZLE "\xef\x9c\xb8"
#define FONT_AWESOME_CLOUD_FOG "\xef\x9d\x8e"
#define FONT_AWESOME_CLOUD_RAIN "\xef\x9c\xbd"
#define FONT_AWESOME_CLOUD_SHOWERS "\xef\x9c\xbf"
#define FONT_AWESOME_CLOUD_SHOWERS_HEAVY "\xef\x9d\x80"
#define FONT_AWESOME_SNOWFLAKE "\xef\x8b\x9c"
#define FONT_AWESOME_SNOWFLAKES "\xef\x9f\x8f"
#define FONT_AWESOME_SMOG "\xef\x9d\x9f"
#define FONT_AWESOME_WIND "\xef\x9c\xae"
#define FONT_AWESOME_HURRICANE "\xef\x9d\x91"
#define FONT_AWESOME_TORNADO "\xef\x9d\xaf"
// 表情符号
#define FONT_AWESOME_NEUTRAL "\xef\x96\xa4"
#define FONT_AWESOME_HAPPY "\xef\x84\x98"
#define FONT_AWESOME_LAUGHING "\xef\x96\x9b"
#define FONT_AWESOME_FUNNY "\xef\x96\x88"
#define FONT_AWESOME_SAD "\xee\x8e\x84"
#define FONT_AWESOME_ANGRY "\xef\x95\x96"
#define FONT_AWESOME_CRYING "\xef\x96\xb3"
#define FONT_AWESOME_LOVING "\xef\x96\x84"
#define FONT_AWESOME_EMBARRASSED "\xef\x95\xb9"
#define FONT_AWESOME_SURPRISED "\xee\x8d\xab"
#define FONT_AWESOME_SHOCKED "\xee\x8d\xb5"
#define FONT_AWESOME_THINKING "\xee\x8e\x9b"
#define FONT_AWESOME_WINKING "\xef\x93\x9a"
#define FONT_AWESOME_COOL "\xee\x8e\x98"
#define FONT_AWESOME_RELAXED "\xee\x8e\x92"
#define FONT_AWESOME_DELICIOUS "\xee\x8d\xb2"
#define FONT_AWESOME_KISSY "\xef\x96\x98"
#define FONT_AWESOME_CONFIDENT "\xee\x90\x89"
#define FONT_AWESOME_SLEEPY "\xee\x8e\x8d"
#define FONT_AWESOME_SILLY "\xee\x8e\xa4"
#define FONT_AWESOME_CONFUSED "\xee\x8d\xad"
// 电池图标
#define FONT_AWESOME_BATTERY_FULL "\xef\x89\x80"
#define FONT_AWESOME_BATTERY_THREE_QUARTERS "\xef\x89\x81"
#define FONT_AWESOME_BATTERY_HALF "\xef\x89\x82"
#define FONT_AWESOME_BATTERY_QUARTER "\xef\x89\x83"
#define FONT_AWESOME_BATTERY_EMPTY "\xef\x89\x84"
#define FONT_AWESOME_BATTERY_SLASH "\xef\x8d\xb7"
#define FONT_AWESOME_BATTERY_BOLT "\xef\x8d\xb6"
// 信号图标
#define FONT_AWESOME_SIGNAL "\xef\x80\x92"
#define FONT_AWESOME_SIGNAL_STRONG "\xef\x9a\x8f"
#define FONT_AWESOME_SIGNAL_GOOD "\xef\x9a\x8e"
#define FONT_AWESOME_SIGNAL_FAIR "\xef\x9a\x8d"
#define FONT_AWESOME_SIGNAL_WEAK "\xef\x9a\x8c"
#define FONT_AWESOME_SIGNAL_OFF "\xef\x9a\x95"
// WiFi图标
#define FONT_AWESOME_WIFI "\xef\x87\xab"
#define FONT_AWESOME_WIFI_FAIR "\xef\x9a\xab"
#define FONT_AWESOME_WIFI_WEAK "\xef\x9a\xaa"
#define FONT_AWESOME_WIFI_SLASH "\xef\x9a\xac"

// 运行时图标查找
const char* font_awesome_get_utf8(const char* name);
```

## 使用示例

### 在 LVGL 中使用图标

```c
#include "font_awesome.h"
#include "display_utils.h"

// 使用预定义宏
lv_obj_t *wifi_label = display_create_label(lv_scr_act(), TEXT_TYPE_ICON, FONT_AWESOME_WIFI);
lv_obj_align(wifi_label, LV_ALIGN_TOP_RIGHT, 0, 0);

// 混合显示
lv_obj_t *rich_text = display_create_rich_text(lv_scr_act(), "WiFi " FONT_AWESOME_WIFI " 已连接");
lv_obj_align(rich_text, LV_ALIGN_CENTER, 0, 0);
```

### 使用运行时查找

```c
#include "font_awesome.h"

const char *wifi_icon = font_awesome_get_utf8("wifi");
if (wifi_icon) {
    lv_label_set_text(label, wifi_icon);
}
```

## 字体数据格式

所有字体均采用 LVGL 的自定义字体格式，包含以下信息：
- 字体名称
- 字符宽度和高度
- 字符位图画布尺寸
- 字符数据（位图像素）
- 字符间距和行间距

## 注意事项

- **内存占用**：中文字体数据较大，建议仅包含常用字符以减小固件体积
- **字体大小**：当前版本仅支持 14x14 尺寸，如需其他尺寸需重新生成字体文件
- **图标编码**：Font Awesome 图标使用 UTF-8 编码，需确保源码文件保存为 UTF-8 格式

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0.0 | 2026-06-29 | 初始版本，包含 Font Awesome 14x14 和中文基础字体 14x14 |

## 许可证

MIT License