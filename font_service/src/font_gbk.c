#include "font_gbk.h"
#include "font_gbk_data.h"
#include "font_service.h"
#include <string.h>

typedef struct {
    FontService base;
} FontGbk;

static int font_gbk_get_font(FontService *self, uint32_t unicode, uint8_t *buf) {
    if (unicode < 0x4E00 || unicode > 0x9FA5) {
        return -1;
    }
    
    uint32_t idx = unicode - 0x4E00;
    uint16_t gbk_code = unicode_to_gbk[idx];
    
    if (gbk_code == 0) {
        return -1;
    }
    
    uint32_t offset = idx * GBK_FONT_BYTES;
    memcpy(buf, &gbk_font_data[offset], GBK_FONT_BYTES);
    
    return 0;
}

static int font_gbk_get_font_size(FontService *self) {
    return GBK_FONT_BYTES;
}

static int font_gbk_get_max_chars(FontService *self) {
    return GBK_FONT_TOTAL_COUNT;
}

FontService *font_gbk_create(void) {
    static FontGbk instance;
    instance.base.type = FONT_TYPE_GBK;
    instance.base.name = "GBK";
    instance.base.get_font = font_gbk_get_font;
    instance.base.get_font_size = font_gbk_get_font_size;
    instance.base.get_max_chars = font_gbk_get_max_chars;
    return &instance.base;
}