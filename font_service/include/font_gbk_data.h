#ifndef FONT_GBK_DATA_H
#define FONT_GBK_DATA_H

#include <stdint.h>

#define GBK_FONT_TOTAL_COUNT 20902
#define GBK_FONT_BYTES 32

extern const uint8_t _binary_unicode_to_gbk_bin_start[];
extern const uint8_t _binary_unicode_to_gbk_bin_end[];
extern const uint8_t _binary_gbk_font_data_bin_start[];
extern const uint8_t _binary_gbk_font_data_bin_end[];

#define unicode_to_gbk ((const uint16_t *)_binary_unicode_to_gbk_bin_start)
#define gbk_font_data _binary_gbk_font_data_bin_start

#endif