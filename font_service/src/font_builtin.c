#include "font_builtin.h"
#include "longyin_column_lsb_font_data.h"
#include "font_service.h"
#include <string.h>

typedef struct {
    FontService base;
} FontBuiltin;

static int font_builtin_get_font(FontService *self, uint32_t unicode, uint8_t *buf) {
    if (unicode < LONGYIN_COLUMN_LSB_FIRST_UNICODE || unicode > LONGYIN_COLUMN_LSB_LAST_UNICODE) {
        return -1;
    }
    const uint8_t *glyph = longyin_column_lsb_glyph(unicode);
    int has_glyph = 0;
    for (size_t i = 0; i < LONGYIN_COLUMN_LSB_FONT_BYTES; i++) {
        if (glyph[i] != 0) { has_glyph = 1; break; }
    }
    if (!has_glyph) return -1;
    memcpy(buf, glyph, LONGYIN_COLUMN_LSB_FONT_BYTES);
    return 0;
}

static int font_builtin_get_font_size(FontService *self) {
    return LONGYIN_COLUMN_LSB_FONT_BYTES;
}

static int font_builtin_get_max_chars(FontService *self) {
    return LONGYIN_COLUMN_LSB_FONT_TOTAL_COUNT;
}

FontService *font_builtin_create(void) {
    static FontBuiltin instance;
    instance.base.type = FONT_TYPE_BUILTIN;
    instance.base.name = "Builtin";
    instance.base.get_font = font_builtin_get_font;
    instance.base.get_font_size = font_builtin_get_font_size;
    instance.base.get_max_chars = font_builtin_get_max_chars;
    return &instance.base;
}
