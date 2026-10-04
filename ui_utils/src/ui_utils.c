/**
 * @file ui_utils.c
 * @brief UI 工具函数实现文件
 */

#include "ui_utils.h"
#include <string.h>

static void draw_wifi_icon_internal(lv_obj_t *canvas, int x, int y, int size, wifi_signal_level_t signal_level) {
    lv_color_t color = lv_color_white();
    
    int center_x = x + size / 2;
    int center_y = y + size / 2;
    
    const int arc_count = 3;
    const int start_angle = 180;
    const int end_angle = 0;
    const int arc_width = 2;
    
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_black();
    rect_dsc.bg_opa = LV_OPA_COVER;
    
    lv_area_t area;
    area.x1 = x;
    area.y1 = y;
    area.x2 = x + size - 1;
    area.y2 = y + size - 1;
    lv_draw_rect(&layer, &rect_dsc, &area);
    
    if (signal_level >= WIFI_SIGNAL_NONE) {
        lv_draw_arc_dsc_t arc_dsc;
        lv_draw_arc_dsc_init(&arc_dsc);
        arc_dsc.color = color;
        arc_dsc.width = size / 4;
        arc_dsc.radius = size / 8;
        arc_dsc.start_angle = 0;
        arc_dsc.end_angle = 360;
        arc_dsc.center.x = center_x;
        arc_dsc.center.y = center_y;
        lv_draw_arc(&layer, &arc_dsc);
    }
    
    for (int i = 0; i < arc_count; i++) {
        int radius = (size / 2) - (i * (size / (arc_count * 2)));
        
        if ((int)signal_level > i) {
            lv_draw_arc_dsc_t arc_dsc;
            lv_draw_arc_dsc_init(&arc_dsc);
            arc_dsc.color = color;
            arc_dsc.width = arc_width;
            arc_dsc.radius = radius;
            arc_dsc.start_angle = start_angle;
            arc_dsc.end_angle = end_angle;
            arc_dsc.center.x = center_x;
            arc_dsc.center.y = center_y;
            lv_draw_arc(&layer, &arc_dsc);
        }
    }
    
    lv_canvas_finish_layer(canvas, &layer);
}

void ui_draw_wifi_icon(lv_obj_t *canvas, int x, int y, int size, wifi_signal_level_t signal_level) {
    draw_wifi_icon_internal(canvas, x, y, size, signal_level);
}

lv_obj_t* ui_create_wifi_icon(lv_obj_t *parent, int x, int y, int size, wifi_signal_level_t signal_level) {
    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_obj_set_size(canvas, size, size);
    lv_obj_set_pos(canvas, x, y);
    
    size_t buf_size = (size * size + 7) / 8;
    lv_color_t *buf = (lv_color_t *)malloc(buf_size);
    if (buf == NULL) {
        lv_obj_del(canvas);
        return NULL;
    }
    
    lv_canvas_set_buffer(canvas, buf, size, size, LV_COLOR_FORMAT_I1);
    lv_obj_set_user_data(canvas, buf);
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
    draw_wifi_icon_internal(canvas, 0, 0, size - 4, signal_level);
    
    return canvas;
}

void ui_update_wifi_icon(lv_obj_t *canvas, wifi_signal_level_t signal_level) {
    if (canvas == NULL) {
        return;
    }
    
    lv_coord_t size = lv_obj_get_width(canvas);
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
    draw_wifi_icon_internal(canvas, 0, 0, size - 4, signal_level);
}

lv_obj_t* ui_create_chinese_label(lv_obj_t *parent, const char *text, 
                                  const lv_font_t *font, lv_align_t align,
                                  int x_offset, int y_offset) {
    lv_obj_t *label = lv_label_create(parent);
    
    if (text != NULL) {
        lv_label_set_text(label, text);
    }
    
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    
    lv_obj_align(label, align, x_offset, y_offset);
    
    return label;
}

void ui_set_label_text(lv_obj_t *label, const char *text) {
    if (label == NULL || text == NULL) {
        return;
    }
    lv_label_set_text(label, text);
}

void ui_set_label_font(lv_obj_t *label, const lv_font_t *font) {
    if (label == NULL || font == NULL) {
        return;
    }
    lv_obj_set_style_text_font(label, font, 0);
}

void ui_set_label_align(lv_obj_t *label, lv_align_t align, int x_offset, int y_offset) {
    if (label == NULL) {
        return;
    }
    lv_obj_align(label, align, x_offset, y_offset);
}

void ui_set_obj_pos(lv_obj_t *obj, int x, int y) {
    if (obj == NULL) {
        return;
    }
    lv_obj_set_pos(obj, x, y);
}

void ui_set_obj_size(lv_obj_t *obj, int width, int height) {
    if (obj == NULL) {
        return;
    }
    lv_obj_set_size(obj, width, height);
}
