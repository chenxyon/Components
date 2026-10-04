#include "font_service.h"

static FontLcdOps g_lcd_ops = {0};

void font_display_set_lcd_ops(const FontLcdOps *ops) {
    if (ops) {
        g_lcd_ops.set_window = ops->set_window;
        g_lcd_ops.write_data16 = ops->write_data16;
    }
}

int font_display_char(FontService *font, uint16_t x, uint16_t y, uint32_t unicode, uint16_t color) {
    if (!font || !g_lcd_ops.set_window || !g_lcd_ops.write_data16) {
        return -1;
    }

    uint8_t font_buf[FONT_BYTES];
    if (font->get_font(font, unicode, font_buf) != 0) {
        return -1;
    }

    g_lcd_ops.set_window(x, y, x + FONT_WIDTH - 1, y + FONT_HEIGHT - 1);

    for (uint8_t t = 0; t < FONT_BYTES; t += 2) {
        uint16_t temp = font_buf[t] << 8 | font_buf[t + 1];
        for (uint8_t t1 = 0; t1 < FONT_WIDTH; t1++) {
            g_lcd_ops.write_data16((temp & 0x8000) ? color : 0x0000);
            temp <<= 1;
        }
    }

    return 0;
}

int font_display_string(FontService *font, uint16_t x, uint16_t y, const uint16_t *str, uint16_t color) {
    if (!font || !str || !g_lcd_ops.set_window || !g_lcd_ops.write_data16) {
        return -1;
    }

    uint16_t current_x = x;
    uint16_t current_y = y;

    while (*str != 0) {
        font_display_char(font, current_x, current_y, *str, color);
        current_x += FONT_WIDTH;

        if (current_x + FONT_WIDTH > 320) {
            current_x = x;
            current_y += FONT_HEIGHT;
        }

        str++;
    }

    return 0;
}