/**
 * @file desktop_grid.h
 * @brief 桌面网格UI组件 - 类似安卓桌面的应用图标布局
 * @version 1.0.0
 * @date 2026-07-25
 */

#ifndef DESKTOP_GRID_H
#define DESKTOP_GRID_H

#include "lvgl.h"

/* ========== 版本定义 ========== */
#define DESKTOP_GRID_VERSION "1.0.0"

/* ========== 网格项结构 ========== */

/**
 * @brief 桌面图标项
 */
typedef struct {
    const char *label;           /* 图标下方文字 */
    const lv_img_dsc_t *icon;    /* 图标图片（可选） */
    lv_color_t icon_bg_color;    /* 图标背景色 */
    lv_color_t label_color;      /* 文字颜色 */
    uint8_t icon_size;           /* 图标大小（像素） */
} desktop_icon_item_t;

/**
 * @brief 桌面网格配置
 */
typedef struct {
    uint8_t cols;                /* 列数（默认4） */
    uint8_t rows;                /* 行数（默认4） */
    uint16_t padding;            /* 内边距（默认10） */
    uint16_t spacing;            /* 网格间距（默认8） */
    lv_color_t bg_color;         /* 背景色 */
    uint8_t icon_size;           /* 默认图标大小 */
    uint8_t font_size;           /* 默认字体大小 */
} desktop_grid_config_t;

/* ========== 公共接口 ========== */

/**
 * @brief 创建桌面网格
 * @param parent 父容器
 * @param config 网格配置（NULL使用默认配置）
 * @return 桌面网格对象指针
 */
lv_obj_t *desktop_grid_create(lv_obj_t *parent, const desktop_grid_config_t *config);

/**
 * @brief 设置网格布局
 * @param grid 桌面网格对象
 * @param cols 列数
 * @param rows 行数
 */
void desktop_grid_set_layout(lv_obj_t *grid, uint8_t cols, uint8_t rows);

/**
 * @brief 添加图标项到网格
 * @param grid 桌面网格对象
 * @param item 图标项配置
 * @param index 位置索引（从0开始，按行优先），-1表示追加到末尾
 * @return 图标项对象指针（lv_obj_t*）
 */
lv_obj_t *desktop_grid_add_icon(lv_obj_t *grid, const desktop_icon_item_t *item, int32_t index);

/**
 * @brief 移除图标项
 * @param grid 桌面网格对象
 * @param index 位置索引
 */
void desktop_grid_remove_icon(lv_obj_t *grid, uint32_t index);

/**
 * @brief 更新图标项
 * @param grid 桌面网格对象
 * @param index 位置索引
 * @param item 新的图标项配置
 */
void desktop_grid_update_icon(lv_obj_t *grid, uint32_t index, const desktop_icon_item_t *item);

/**
 * @brief 清空所有图标项
 * @param grid 桌面网格对象
 */
void desktop_grid_clear(lv_obj_t *grid);

/**
 * @brief 获取图标项数量
 * @param grid 桌面网格对象
 * @return 图标项数量
 */
uint32_t desktop_grid_get_icon_count(lv_obj_t *grid);

/**
 * @brief 设置网格背景色
 * @param grid 桌面网格对象
 * @param color 背景色
 */
void desktop_grid_set_bg_color(lv_obj_t *grid, lv_color_t color);

/**
 * @brief 设置网格间距
 * @param grid 桌面网格对象
 * @param spacing 间距（像素）
 */
void desktop_grid_set_spacing(lv_obj_t *grid, uint16_t spacing);

/**
 * @brief 设置网格内边距
 * @param grid 桌面网格对象
 * @param padding 内边距（像素）
 */
void desktop_grid_set_padding(lv_obj_t *grid, uint16_t padding);

/**
 * @brief 设置图标大小
 * @param grid 桌面网格对象
 * @param icon_size 图标大小（像素）
 */
void desktop_grid_set_icon_size(lv_obj_t *grid, uint8_t icon_size);

/**
 * @brief 获取默认配置
 * @return 默认配置结构体
 */
const desktop_grid_config_t *desktop_grid_get_default_config(void);

#endif // DESKTOP_GRID_H
