#ifndef YUESONG_FONT_DATA_H
#define YUESONG_FONT_DATA_H

#include <stdint.h>

#define FONT_WIDTH 16
#define FONT_HEIGHT 16
#define YUESONG_FONT_TOTAL_COUNT 20902
#define YUESONG_FONT_BYTES 32

extern const uint8_t _binary_yuesong_unicode_to_gbk_bin_start[];
extern const uint8_t _binary_yuesong_unicode_to_gbk_bin_end[];
extern const uint8_t _binary_yuesong_font_data_bin_start[];
extern const uint8_t _binary_yuesong_font_data_bin_end[];

#define yuesong_unicode_to_gbk ((const uint16_t *)_binary_yuesong_unicode_to_gbk_bin_start)
#define yuesong_font_data _binary_yuesong_font_data_bin_start

#endif
