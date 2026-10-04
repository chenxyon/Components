#include "font_w25q.h"
#include "font_service.h"
#include <stddef.h>

typedef struct {
    FontService base;
    uint32_t font_offset;
    uint32_t font_count;
} FontW25q;

static int font_w25q_get_font(FontService *self, uint32_t unicode, uint8_t *buf) {
    FontW25q *this = (FontW25q *)self;
    
    if (unicode < 0x4E00 || unicode > 0x9FA5) {
        return -1;
    }
    
    uint32_t offset = this->font_offset + (unicode - 0x4E00) * FONT_BYTES;
    
    return -1;
}

static int font_w25q_get_font_size(FontService *self) {
    return FONT_BYTES;
}

static int font_w25q_get_max_chars(FontService *self) {
    FontW25q *this = (FontW25q *)self;
    return this->font_count;
}

FontService *font_w25q_create(void) {
    static FontW25q instance;
    instance.base.type = FONT_TYPE_W25Q;
    instance.base.name = "W25Q";
    instance.base.get_font = font_w25q_get_font;
    instance.base.get_font_size = font_w25q_get_font_size;
    instance.base.get_max_chars = font_w25q_get_max_chars;
    instance.font_offset = 0x000000;
    instance.font_count = 21003;
    return &instance.base;
}

void font_w25q_init(void) {
}