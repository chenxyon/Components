#include "display_utils.h"
#include "lvgl_port.h"
#include <lvgl.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <esp_err.h>
#include <time.h>
#include <sys/time.h>

#define MAX_TEXT_LEN 512

static text_type_t detect_text_type(const char *text);
static lv_obj_t* create_label_with_font(lv_obj_t *parent, text_type_t type, const char *text);

void clear_screen(lv_obj_t *parent)
{
    lv_obj_clean(parent);
}

lv_obj_t* display_chinese_text(lv_obj_t *parent, const char *text)
{
    return create_label_with_font(parent, TEXT_TYPE_CHINESE, text);
}

lv_obj_t* display_english_text(lv_obj_t *parent, const char *text)
{
    return create_label_with_font(parent, TEXT_TYPE_ENGLISH, text);
}

static text_type_t detect_text_type(const char *text)
{
    if (!text || *text == '\0') {
        return TEXT_TYPE_ENGLISH;
    }

    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        if (*p >= 0xE4 && *p <= 0xE9) {
            return TEXT_TYPE_CHINESE;
        }
        if (*p == 0xEF && (*(p+1) & 0x80) && (*(p+2) & 0x80)) {
            uint16_t code = ((*(p+1) & 0x3F) << 6) | (*(p+2) & 0x3F);
            if (code >= 0xF000 && code <= 0xFFFF) {
                return TEXT_TYPE_ICON;
            }
        }
        if (*p >= 0xF0 && *p <= 0xFF) {
            return TEXT_TYPE_ICON;
        }
        if (*p >= 0x80) {
            p++;
            if (*p) p++;
            if (*p) p++;
            continue;
        }
        p++;
    }
    return TEXT_TYPE_ENGLISH;
}

text_type_t display_detect_text_type(const char *text)
{
    return detect_text_type(text);
}

static lv_obj_t* create_label_with_font(lv_obj_t *parent, text_type_t type, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    return label;
}

lv_obj_t* display_create_label(lv_obj_t *parent, text_type_t type, const char *text)
{
    if (type == TEXT_TYPE_AUTO) {
        type = detect_text_type(text);
    }
    return create_label_with_font(parent, type, text);
}

lv_obj_t* display_create_label_fmt(lv_obj_t *parent, text_type_t type, const char *format, ...)
{
    char buffer[MAX_TEXT_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, MAX_TEXT_LEN, format, args);
    va_end(args);

    if (type == TEXT_TYPE_AUTO) {
        type = detect_text_type(buffer);
    }
    return create_label_with_font(parent, type, buffer);
}

lv_obj_t* display_create_rich_text(lv_obj_t *parent, const char *text)
{
    if (!text || *text == '\0') {
        return NULL;
    }

    lv_obj_t *container = lv_obj_create(parent);
    if (!container) {
        return NULL;
    }
    lv_obj_set_size(container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);

    const unsigned char *p = (const unsigned char *)text;

    while (*p) {
        text_type_t type = detect_text_type((const char *)p);
        
        const unsigned char *end = p;
        if (type == TEXT_TYPE_CHINESE) {
            while (*end && *end >= 0xE4 && *end <= 0xE9) {
                end += 3;
            }
        } else if (type == TEXT_TYPE_ICON) {
            if (*end == 0xEF && (*(end+1) & 0x80) && (*(end+2) & 0x80)) {
                end += 3;
            } else if (*end >= 0xF0 && *end <= 0xFF) {
                end += 4;
            } else {
                end++;
            }
        } else {
            while (*end && *end < 0x80) {
                end++;
            }
        }

        if (end > p) {
            char segment[MAX_TEXT_LEN];
            size_t len = end - p;
            if (len >= MAX_TEXT_LEN) len = MAX_TEXT_LEN - 1;
            memcpy(segment, p, len);
            segment[len] = '\0';

            lv_obj_t *label = lv_label_create(container);
            lv_label_set_text(label, segment);
        }

        p = end;
    }

    return container;
}

lv_obj_t* display_create_rich_text_fmt(lv_obj_t *parent, const char *format, ...)
{
    char buffer[MAX_TEXT_LEN];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, MAX_TEXT_LEN, format, args);
    va_end(args);

    return display_create_rich_text(parent, buffer);
}
