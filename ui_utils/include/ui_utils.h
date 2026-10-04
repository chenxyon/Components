/**
 * @file ui_utils.h
 * @brief UI 工具函数头文件
 */

#ifndef UI_UTILS_H
#define UI_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>

typedef enum {
    WIFI_SIGNAL_NONE = 0,
    WIFI_SIGNAL_WEAK,
    WIFI_SIGNAL_MEDIUM,
    WIFI_SIGNAL_STRONG,
} wifi_signal_level_t;

void ui_draw_wifi_icon(lv_obj_t *canvas, int x, int y, int size, wifi_signal_level_t signal_level);
lv_obj_t* ui_create_wifi_icon(lv_obj_t *parent, int x, int y, int size, wifi_signal_level_t signal_level);
void ui_update_wifi_icon(lv_obj_t *canvas, wifi_signal_level_t signal_level);

lv_obj_t* ui_create_chinese_label(lv_obj_t *parent, const char *text, 
                                  const lv_font_t *font, lv_align_t align,
                                  int x_offset, int y_offset);
void ui_set_label_text(lv_obj_t *label, const char *text);
void ui_set_label_font(lv_obj_t *label, const lv_font_t *font);
void ui_set_label_align(lv_obj_t *label, lv_align_t align, int x_offset, int y_offset);

void ui_set_obj_pos(lv_obj_t *obj, int x, int y);
void ui_set_obj_size(lv_obj_t *obj, int width, int height);

#ifdef __cplusplus
}
#endif

#endif // UI_UTILS_H
