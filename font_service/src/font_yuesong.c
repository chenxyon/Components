#include "font_yuesong.h"
#include "yuesong_font_data.h"
#include "font_service.h"
#include <string.h>

typedef struct {
    FontService base;
} FontYuesong;

static int font_yuesong_get_font(FontService *self, uint32_t unicode, uint8_t *buf) {
    if (unicode < 0x4E00 || unicode > 0x9FA5) {
        return -1;
    }
    
    uint32_t idx = unicode - 0x4E00;
    uint16_t gbk_code = yuesong_unicode_to_gbk[idx];
    
    if (gbk_code == 0) {
        return -1;
    }
    
    uint32_t offset = idx * YUESONG_FONT_BYTES;
    memcpy(buf, &yuesong_font_data[offset], YUESONG_FONT_BYTES);
    
    return 0;
}

static int font_yuesong_get_font_size(FontService *self) {
    return YUESONG_FONT_BYTES;
}

static int font_yuesong_get_max_chars(FontService *self) {
    return YUESONG_FONT_TOTAL_COUNT;
}

FontService *font_yuesong_create(void) {
    static FontYuesong instance;
    instance.base.type = FONT_TYPE_YUESONG;
    instance.base.name = "Yuesong";
    instance.base.get_font = font_yuesong_get_font;
    instance.base.get_font_size = font_yuesong_get_font_size;
    instance.base.get_max_chars = font_yuesong_get_max_chars;
    return &instance.base;
}