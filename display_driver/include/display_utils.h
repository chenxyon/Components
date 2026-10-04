#ifndef DISPLAY_UTILS_H
#define DISPLAY_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include <esp_err.h>
#include "display_driver.h"

void clear_screen(lv_obj_t *parent);
lv_obj_t* display_chinese_text(lv_obj_t *parent, const char *text);
lv_obj_t* display_english_text(lv_obj_t *parent, const char *text);

text_type_t display_detect_text_type(const char *text);

lv_obj_t* display_create_label(lv_obj_t *parent, text_type_t type, const char *text);
lv_obj_t* display_create_label_fmt(lv_obj_t *parent, text_type_t type, const char *format, ...);

lv_obj_t* display_create_rich_text(lv_obj_t *parent, const char *text);
lv_obj_t* display_create_rich_text_fmt(lv_obj_t *parent, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
