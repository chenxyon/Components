#ifndef FONT_SERVICE_H
#define FONT_SERVICE_H

#include <stdint.h>

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define FONT_BYTES (FONT_WIDTH * FONT_HEIGHT / 8)

typedef enum {
    FONT_TYPE_BUILTIN,
    FONT_TYPE_GBK,
    FONT_TYPE_YUESONG,
    FONT_TYPE_W25Q,
    FONT_TYPE_MAX
} font_type_t;

typedef struct FontService FontService;

struct FontService {
    font_type_t type;
    const char *name;
    int (*get_font)(FontService *self, uint32_t unicode, uint8_t *buf);
    int (*get_font_size)(FontService *self);
    int (*get_max_chars)(FontService *self);
};

typedef void (*lcd_set_window_t)(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
typedef void (*lcd_write_data16_t)(uint16_t data);

typedef struct {
    lcd_set_window_t set_window;
    lcd_write_data16_t write_data16;
} FontLcdOps;

FontService *font_service_create(font_type_t type);

void font_display_set_lcd_ops(const FontLcdOps *ops);
int font_display_char(FontService *font, uint16_t x, uint16_t y, uint32_t unicode, uint16_t color);
int font_display_string(FontService *font, uint16_t x, uint16_t y, const uint16_t *str, uint16_t color);

#endif