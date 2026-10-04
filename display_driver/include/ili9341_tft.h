/**
 * @file ili9341_tft.h
 * @brief ILI9341 TFT屏幕驱动 (2.4寸)
 * @version 1.0.0
 * @date 2026-06-28
 * 
 * 支持型号：
 * - DISPLAY_TYPE_TFT_24_ILI9341 (320x240 横屏)
 * - DISPLAY_TYPE_TFT_24_ILI9341_VERT (240x320 竖屏)
 * 
 * 特性：
 * - 支持LVGL显示框架
 * - RGB565颜色格式
 * - SPI通信接口
 * - 可配置旋转方向
 */

#ifndef ILI9341_TFT_H
#define ILI9341_TFT_H

#include "display_driver.h"

/* ========== 驱动版本 ========== */
#define ILI9341_DRIVER_VERSION "1.0.0"

/* ========== 屏幕参数 ========== */
// 默认屏幕尺寸，可通过 Kconfig 配置覆盖
#ifndef CONFIG_DISPLAY_WIDTH
#define CONFIG_DISPLAY_WIDTH 320
#endif
#ifndef CONFIG_DISPLAY_HEIGHT
#define CONFIG_DISPLAY_HEIGHT 240
#endif

// 横屏模式 (width x height)
#define ILI9341_HOR_WIDTH   CONFIG_DISPLAY_WIDTH
#define ILI9341_HOR_HEIGHT  CONFIG_DISPLAY_HEIGHT

// 竖屏模式 (height x width)
#define ILI9341_VERT_WIDTH  CONFIG_DISPLAY_HEIGHT
#define ILI9341_VERT_HEIGHT CONFIG_DISPLAY_WIDTH

// SPI时钟频率
#define ILI9341_SPI_FREQ    60000000  // 60MHz

// 颜色深度
#define ILI9341_COLOR_DEPTH 16  // RGB565

/* ========== 旋转方向配置 ========== */
typedef enum {
    ILI9341_ROTATE_0   = 0x00,  // 竖屏 (240x320)
    ILI9341_ROTATE_90  = 0x60,  // 横屏逆时针90度 (320x240)
    ILI9341_ROTATE_180 = 0xC0,  // 竖屏翻转 (240x320)
    ILI9341_ROTATE_270 = 0xA0,  // 横屏顺时针90度 (320x240)
} ili9341_rotate_t;

/* ========== 默认引脚配置 ========== */
// ESP32-S3 默认引脚配置
// 使用 Kconfig 配置，若未定义则使用默认值
#ifndef CONFIG_DISPLAY_SPI_MOSI
#define CONFIG_DISPLAY_SPI_MOSI 11
#endif
#ifndef CONFIG_DISPLAY_SPI_MISO
#define CONFIG_DISPLAY_SPI_MISO -1
#endif
#ifndef CONFIG_DISPLAY_SPI_SCLK
#define CONFIG_DISPLAY_SPI_SCLK 12
#endif
#ifndef CONFIG_DISPLAY_SPI_CS
#define CONFIG_DISPLAY_SPI_CS 10
#endif
#ifndef CONFIG_DISPLAY_SPI_DC
#define CONFIG_DISPLAY_SPI_DC 9
#endif
#ifndef CONFIG_DISPLAY_SPI_RST
#define CONFIG_DISPLAY_SPI_RST 4
#endif
#ifndef CONFIG_DISPLAY_SPI_BL
#define CONFIG_DISPLAY_SPI_BL -1
#endif
#ifndef CONFIG_DISPLAY_SPI_FREQ
#define CONFIG_DISPLAY_SPI_FREQ 4
#endif

#define ILI9341_DEFAULT_PINS {\
    .mosi = CONFIG_DISPLAY_SPI_MOSI, \
    .miso = CONFIG_DISPLAY_SPI_MISO, \
    .sclk = CONFIG_DISPLAY_SPI_SCLK, \
    .cs   = CONFIG_DISPLAY_SPI_CS, \
    .dc   = CONFIG_DISPLAY_SPI_DC, \
    .rst  = CONFIG_DISPLAY_SPI_RST, \
    .bl   = CONFIG_DISPLAY_SPI_BL, \
    .i2c_sda = -1, \
    .i2c_scl = -1, \
    .i2c_addr = -1 \
}

/* ========== 公共接口 ========== */

/**
 * @brief 初始化ILI9341驱动
 * @param type 屏幕型号 (DISPLAY_TYPE_TFT_24_ILI9341 或 DISPLAY_TYPE_TFT_24_ILI9341_VERT)
 * @param pins 引脚配置，NULL则使用默认配置
 * @return ESP_OK成功，其他值失败
 */
esp_err_t ili9341_tft_init(display_type_t type, const display_pin_config_t *pins);

/**
 * @brief 反初始化ILI9341驱动
 * @return ESP_OK成功，其他值失败
 */
esp_err_t ili9341_tft_deinit(void);

/**
 * @brief LVGL flush回调函数
 * @param x1 区域左边界
 * @param y1 区域上边界
 * @param x2 区域右边界
 * @param y2 区域下边界
 * @param color_p 颜色数据指针 (RGB565格式)
 */
void ili9341_tft_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint8_t *color_p);

/**
 * @brief 获取屏幕宽度
 * @return 屏幕宽度 (像素)
 */
int ili9341_tft_get_width(void);

/**
 * @brief 获取屏幕高度
 * @return 屏幕高度 (像素)
 */
int ili9341_tft_get_height(void);

/**
 * @brief 判断是否为单色屏幕
 * @return false (ILI9341为彩色屏幕)
 */
bool ili9341_tft_is_monochrome(void);

/**
 * @brief 绘制单个像素
 * @param x X坐标
 * @param y Y坐标
 * @param color RGB565颜色值
 */
void ili9341_tft_draw_pixel(int16_t x, int16_t y, uint16_t color);

/**
 * @brief 填充矩形区域
 * @param x1 左上角X坐标
 * @param y1 左上角Y坐标
 * @param x2 右下角X坐标
 * @param y2 右下角Y坐标
 * @param color RGB565颜色值
 */
void ili9341_tft_fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

/**
 * @brief 设置背光亮度
 * @param brightness 亮度值 (0-100)
 * @return ESP_OK成功，其他值失败
 */
esp_err_t ili9341_tft_set_backlight(uint8_t brightness);

/**
 * @brief 获取ILI9341驱动信息
 * @return 驱动信息结构指针
 */
const display_driver_t *ili9341_tft_get_driver(void);

/**
 * @brief 根据屏幕型号获取对应的驱动
 * @param type 屏幕型号
 * @return 驱动信息结构指针，无效型号返回NULL
 */
const display_driver_t *ili9341_tft_get_driver_by_type(display_type_t type);

/**
 * @brief 导出ILI9341横屏驱动结构体（用于驱动注册表）
 */
extern const display_driver_t ili9341_driver_hor;

/**
 * @brief 导出ILI9341竖屏驱动结构体（用于驱动注册表）
 */
extern const display_driver_t ili9341_driver_vert;

#endif // ILI9341_TFT_H
