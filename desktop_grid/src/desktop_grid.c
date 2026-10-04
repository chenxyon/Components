/**
 * @file desktop_grid.c
 * @brief 桌面网格UI组件 - 实现
 * @version 1.0.0
 * @date 2026-07-25
 */

#include "desktop_grid.h"
#include "esp_log.h"

static const char *TAG = "DESKTOP_GRID";

/* LVGL 9.x 兼容的颜色定义 - 使用 LV_COLOR_MAKE 以便常量初始化 */
#define COLOR_SILVER     LV_COLOR_MAKE(0xC0, 0xC0, 0xC0)

/* ========== 默认配置 ========== */
static const desktop_grid_config_t default_config = {
    .cols = 4,
    .rows = 4,
    .padding = 10,
    .spacing = 8,
    .bg_color = COLOR_SILVER,
    .icon_size = 60,
    .font_size = 12
};

/* ========== 私有数据结构 ========== */
typedef struct {
    uint8_t cols;
    uint8_t rows;
    uint16_t padding;
    uint16_t spacing;
    uint32_t icon_count;
} desktop_grid_data_t;

/* ========== 私有函数声明 ========== */
static void desktop_grid_update_layout(lv_obj_t *grid);
static lv_obj_t *desktop_grid_create_icon_item(lv_obj_t *parent, const desktop_icon_item_t *item);

/* ========== 公共接口实现 ========== */

const desktop_grid_config_t *desktop_grid_get_default_config(void)
{
    return &default_config;
}

lv_obj_t *desktop_grid_create(lv_obj_t *parent, const desktop_grid_config_t *config)
{
    ESP_LOGI(TAG, "Creating desktop grid...");
    
    if (!parent) {
        ESP_LOGE(TAG, "Parent object is NULL");
        return NULL;
    }

    const desktop_grid_config_t *cfg = config ? config : &default_config;

    /* 创建主容器 */
    lv_obj_t *grid = lv_obj_create(parent);
    ESP_LOGI(TAG, "Grid object created: %p", grid);
    if (!grid) {
        ESP_LOGE(TAG, "Failed to create grid object");
        return NULL;
    }

    /* 分配私有数据 */
    desktop_grid_data_t *data = lv_malloc(sizeof(desktop_grid_data_t));
    ESP_LOGI(TAG, "Grid data allocated: %p", data);
    if (!data) {
        ESP_LOGE(TAG, "Failed to allocate grid data");
        lv_obj_del(grid);
        return NULL;
    }

    data->cols = cfg->cols;
    data->rows = cfg->rows;
    data->padding = cfg->padding;
    data->spacing = cfg->spacing;
    data->icon_count = 0;

    lv_obj_set_user_data(grid, data);
    ESP_LOGI(TAG, "User data set");

    /* 设置样式 - 简化以避免触发重绘 */
    lv_obj_set_size(grid, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    ESP_LOGI(TAG, "Size set");
    
    lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(grid, 0, LV_PART_MAIN);
    ESP_LOGI(TAG, "Border and radius set");

    /* 使用 flex 布局 */
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    ESP_LOGI(TAG, "Flex layout set");

    /* 设置间距 */
    lv_obj_set_style_pad_all(grid, cfg->padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid, cfg->spacing, LV_PART_MAIN);
    lv_obj_set_style_pad_column(grid, cfg->spacing, LV_PART_MAIN);
    ESP_LOGI(TAG, "Padding and spacing set");

    /* 设置背景色 - 放在最后 */
    lv_obj_set_style_bg_color(grid, cfg->bg_color, LV_PART_MAIN);
    ESP_LOGI(TAG, "Background color set");

    ESP_LOGI(TAG, "Desktop grid created successfully");
    return grid;
}

void desktop_grid_set_layout(lv_obj_t *grid, uint8_t cols, uint8_t rows)
{
    if (!grid) return;

    desktop_grid_data_t *data = lv_obj_get_user_data(grid);
    if (!data) return;

    data->cols = cols;
    data->rows = rows;

    desktop_grid_update_layout(grid);
}

lv_obj_t *desktop_grid_add_icon(lv_obj_t *grid, const desktop_icon_item_t *item, int32_t index)
{
    if (!grid || !item) return NULL;

    desktop_grid_data_t *data = lv_obj_get_user_data(grid);
    if (!data) return NULL;

    /* 创建图标项 */
    lv_obj_t *icon_item = desktop_grid_create_icon_item(grid, item);
    if (!icon_item) {
        ESP_LOGE(TAG, "Failed to create icon item");
        return NULL;
    }

    data->icon_count++;
    desktop_grid_update_layout(grid);

    return icon_item;
}

void desktop_grid_remove_icon(lv_obj_t *grid, uint32_t index)
{
    if (!grid) return;

    desktop_grid_data_t *data = lv_obj_get_user_data(grid);
    if (!data) return;

    if (index >= data->icon_count) return;

    lv_obj_t *child = lv_obj_get_child(grid, index);
    if (child) {
        lv_obj_del(child);
        data->icon_count--;
        desktop_grid_update_layout(grid);
    }
}

void desktop_grid_update_icon(lv_obj_t *grid, uint32_t index, const desktop_icon_item_t *item)
{
    if (!grid || !item) return;

    desktop_grid_data_t *data = lv_obj_get_user_data(grid);
    if (!data) return;

    if (index >= data->icon_count) return;

    /* 删除旧图标 */
    lv_obj_t *old_child = lv_obj_get_child(grid, index);
    if (old_child) {
        lv_obj_del(old_child);
    }

    /* 创建新图标 */
    lv_obj_t *new_child = desktop_grid_create_icon_item(grid, item);
    if (new_child) {
        desktop_grid_update_layout(grid);
    }
}

void desktop_grid_clear(lv_obj_t *grid)
{
    if (!grid) return;

    desktop_grid_data_t *data = lv_obj_get_user_data(grid);
    if (!data) return;

    lv_obj_clean(grid);
    data->icon_count = 0;
}

uint32_t desktop_grid_get_icon_count(lv_obj_t *grid)
{
    if (!grid) return 0;

    desktop_grid_data_t *data = lv_obj_get_user_data(grid);
    if (!data) return 0;

    return data->icon_count;
}

void desktop_grid_set_bg_color(lv_obj_t *grid, lv_color_t color)
{
    if (!grid) return;
    lv_obj_set_style_bg_color(grid, color, LV_PART_MAIN);
}

void desktop_grid_set_spacing(lv_obj_t *grid, uint16_t spacing)
{
    if (!grid) return;

    desktop_grid_data_t *data = lv_obj_get_user_data(grid);
    if (!data) return;

    data->spacing = spacing;
    desktop_grid_update_layout(grid);
}

void desktop_grid_set_padding(lv_obj_t *grid, uint16_t padding)
{
    if (!grid) return;

    desktop_grid_data_t *data = lv_obj_get_user_data(grid);
    if (!data) return;

    data->padding = padding;
    lv_obj_set_style_pad_all(grid, padding, LV_PART_MAIN);
}

void desktop_grid_set_icon_size(lv_obj_t *grid, uint8_t icon_size)
{
    if (!grid) return;

    /* 更新所有图标的大小 */
    uint32_t child_count = lv_obj_get_child_cnt(grid);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *icon_item = lv_obj_get_child(grid, i);
        if (icon_item) {
            /* 获取图标容器（第一个子对象） */
            lv_obj_t *icon_container = lv_obj_get_child(icon_item, 0);
            if (icon_container) {
                lv_obj_set_size(icon_container, icon_size, icon_size);
                lv_obj_set_style_radius(icon_container, icon_size / 5, LV_PART_MAIN);
            }
        }
    }
}

/* ========== 私有函数实现 ========== */

static void desktop_grid_update_layout(lv_obj_t *grid)
{
    if (!grid) return;

    desktop_grid_data_t *data = lv_obj_get_user_data(grid);
    if (!data) return;

    uint32_t child_count = lv_obj_get_child_cnt(grid);
    if (child_count == 0) return;

    /* 计算每个图标的宽度 */
    lv_coord_t grid_width = lv_obj_get_width(grid);
    lv_coord_t total_padding = data->padding * 2;
    lv_coord_t total_spacing = (data->cols - 1) * data->spacing;
    lv_coord_t item_width = (grid_width - total_padding - total_spacing) / data->cols;

    /* 更新所有子项的大小 */
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(grid, i);
        if (child) {
            lv_obj_set_width(child, item_width);
            lv_obj_set_height(child, LV_SIZE_CONTENT);
        }
    }

    /* 设置间距（LVGL 9.x 使用 padding 代替 gap） */
    lv_obj_set_style_pad_row(grid, data->spacing, LV_PART_MAIN);
    lv_obj_set_style_pad_column(grid, data->spacing, LV_PART_MAIN);
}

static lv_obj_t *desktop_grid_create_icon_item(lv_obj_t *parent, const desktop_icon_item_t *item)
{
    if (!parent || !item) return NULL;

    /* 创建图标项容器 */
    lv_obj_t *icon_item = lv_obj_create(parent);
    if (!icon_item) return NULL;

    /* 设置图标项样式 */
    lv_obj_set_size(icon_item, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(icon_item, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(icon_item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(icon_item, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(icon_item, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(icon_item, 4, LV_PART_MAIN);

    /* 创建图标容器 */
    lv_obj_t *icon_container = lv_obj_create(icon_item);
    if (!icon_container) {
        lv_obj_del(icon_item);
        return NULL;
    }

    uint8_t icon_size = item->icon_size > 0 ? item->icon_size : default_config.icon_size;
    lv_obj_set_size(icon_container, icon_size, icon_size);
    lv_obj_set_style_bg_color(icon_container, item->icon_bg_color, LV_PART_MAIN);
    lv_obj_set_style_radius(icon_container, icon_size / 5, LV_PART_MAIN);
    lv_obj_set_style_border_width(icon_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(icon_container, 4, LV_PART_MAIN);
    lv_obj_center(icon_container);

    /* 添加图标内容 */
    if (item->icon) {
        lv_obj_t *icon_img = lv_img_create(icon_container);
        lv_img_set_src(icon_img, item->icon);
        lv_obj_center(icon_img);
    } else {
        /* 如果没有图片，创建一个简单的圆形或矩形作为占位符 */
        lv_obj_t *placeholder = lv_obj_create(icon_container);
        lv_obj_set_size(placeholder, icon_size / 2, icon_size / 2);
        lv_obj_set_style_bg_color(placeholder, lv_color_lighten(item->icon_bg_color, 30), LV_PART_MAIN);
        lv_obj_set_style_radius(placeholder, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_center(placeholder);
    }

    /* 创建文字标签 */
    if (item->label && item->label[0] != '\0') {
        lv_obj_t *label = lv_label_create(icon_item);
        lv_label_set_text(label, item->label);
        lv_obj_set_style_text_color(label, item->label_color, LV_PART_MAIN);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_width(label, LV_SIZE_CONTENT);
    }

    return icon_item;
}