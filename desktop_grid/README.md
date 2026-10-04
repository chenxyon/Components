# Desktop Grid UI Component

## 概述

桌面网格UI组件，类似安卓桌面的应用图标布局，支持自定义网格行列数（3×3、3×4、4×3、4×4等），支持图标和文字标签显示。

## 特性

- ✅ 支持自定义网格布局（行列数可配置）
- ✅ 支持自定义图标大小、间距、内边距
- ✅ 支持图标背景色和文字颜色配置
- ✅ 支持图标图片或占位符显示
- ✅ 支持动态添加、删除、更新图标
- ✅ LVGL 9.x 兼容

## 版本

- 版本号：1.0.0
- 日期：2026-07-25

## 依赖

- LVGL 图形库

## API 接口

### 创建网格

```c
desktop_grid_config_t config = {
    .cols = 4,
    .rows = 4,
    .padding = 8,
    .spacing = 6,
    .bg_color = lv_color_hex(0xC0C0C0),
    .icon_size = 56,
    .font_size = 10
};
lv_obj_t *grid = desktop_grid_create(lv_scr_act(), &config);
```

### 添加图标

```c
desktop_icon_item_t app = {
    .label = "WeChat",
    .icon = NULL,
    .icon_bg_color = lv_color_hex(0xADD8E6),
    .label_color = lv_color_hex(0x000000),
    .icon_size = 56
};
desktop_grid_add_icon(grid, &app, -1);
```

### 设置布局

```c
// 切换到 3x3 布局
desktop_grid_set_layout(grid, 3, 3);
```

### 清空网格

```c
desktop_grid_clear(grid);
```

### 获取图标数量

```c
uint32_t count = desktop_grid_get_icon_count(grid);
```

## 配置项

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| cols | uint8_t | 4 | 列数 |
| rows | uint8_t | 4 | 行数 |
| padding | uint16_t | 10 | 内边距（像素） |
| spacing | uint16_t | 8 | 网格间距（像素） |
| bg_color | lv_color_t | 银色 | 背景色 |
| icon_size | uint8_t | 60 | 默认图标大小（像素） |
| font_size | uint8_t | 12 | 默认字体大小 |

## 使用示例

```c
#include "desktop_grid.h"

void demo_task(void *arg)
{
    // 创建 4x4 网格
    desktop_grid_config_t config = {
        .cols = 4,
        .rows = 4,
        .padding = 8,
        .spacing = 6,
        .bg_color = lv_color_hex(0xC0C0C0),
        .icon_size = 56
    };
    lv_obj_t *grid = desktop_grid_create(lv_scr_act(), &config);
    lv_obj_set_size(grid, 320, 240);
    lv_obj_center(grid);

    // 添加图标
    const desktop_icon_item_t apps[] = {
        {"WeChat", NULL, lv_color_hex(0xADD8E6), lv_color_hex(0x000000), 56},
        {"QQ",     NULL, lv_color_hex(0x0000FF), lv_color_hex(0xFFFFFF), 56},
        {"Phone",  NULL, lv_color_hex(0x008000), lv_color_hex(0xFFFFFF), 56},
        // ...
    };

    for (int i = 0; i < sizeof(apps) / sizeof(apps[0]); i++) {
        desktop_grid_add_icon(grid, &apps[i], -1);
    }
}
```

## Kconfig 配置

```
menu "Desktop Grid Configuration"
    config DESKTOP_GRID_ENABLE
        bool "Enable Desktop Grid Component"
        default y
    
    config DESKTOP_GRID_DEFAULT_COLS
        int "Default grid columns"
        default 4
    
    config DESKTOP_GRID_DEFAULT_ROWS
        int "Default grid rows"
        default 4
    
    config DESKTOP_GRID_DEFAULT_ICON_SIZE
        int "Default icon size (px)"
        default 60
endmenu
```

## 注意事项

1. 当前版本仅支持显示功能，暂不支持交互（点击、拖拽等）
2. 图标图片需使用 LVGL 支持的图片格式（如 lv_img_dsc_t）
3. 字体使用 LVGL 默认字体 lv_font_montserrat_14

## 许可证

MIT License
