/**
 * @file ssd1306_oled.h
 * @brief SSD1306 OLED屏幕驱动 (0.91寸/0.96寸)
 * @version 1.0.0
 * @date 2026-06-28
 * 
 * 支持型号：
 * - DISPLAY_TYPE_OLED_091_SSD1306 (0.91寸 128x32)
 * - DISPLAY_TYPE_OLED_096_SSD1306 (0.96寸 128x64)
 * 
 * 特性：
 * - 支持LVGL显示框架
 * - I2C通信接口
 * - 单色显示 (1-bit颜色深度)
 */

#ifndef SSD1306_OLED_H
#define SSD1306_OLED_H

#include "display_driver.h"

/* ========== 驱动版本 ========== */
#define SSD1306_DRIVER_VERSION "1.0.0"

/* ========== 屏幕参数 ========== */
// 0.91寸 OLED (128x32)
#define SSD1306_091_WIDTH   128
#define SSD1306_091_HEIGHT  32

// 0.96寸 OLED (128x64)
#define SSD1306_096_WIDTH   128
#define SSD1306_096_HEIGHT  64

// I2C时钟频率
#define SSD1306_I2C_FREQ    400000  // 400kHz

// 颜色深度
#define SSD1306_COLOR_DEPTH 1  // 单色

// I2C地址
#define SSD1306_I2C_ADDR    0x3C  // 默认地址

/* ========== 默认引脚配置 ========== */
// ESP32-S3 默认I2C引脚配置
// 使用 Kconfig 配置，若未定义则使用默认值
#ifndef CONFIG_DISPLAY_I2C_SDA
#define CONFIG_DISPLAY_I2C_SDA 6
#endif
#ifndef CONFIG_DISPLAY_I2C_SCL
#define CONFIG_DISPLAY_I2C_SCL 7
#endif
#ifndef CONFIG_DISPLAY_I2C_ADDR
#define CONFIG_DISPLAY_I2C_ADDR 0x3C
#endif
#ifndef CONFIG_DISPLAY_I2C_FREQ
#define CONFIG_DISPLAY_I2C_FREQ 400
#endif

#define SSD1306_DEFAULT_PINS {\
    .mosi = -1, \
    .miso = -1, \
    .sclk = -1, \
    .cs   = -1, \
    .dc   = -1, \
    .rst  = -1, \
    .bl   = -1, \
    .i2c_sda = CONFIG_DISPLAY_I2C_SDA, \
    .i2c_scl = CONFIG_DISPLAY_I2C_SCL, \
    .i2c_addr = CONFIG_DISPLAY_I2C_ADDR \
}

/* ========== I2C引脚扩展结构 ========== */
typedef struct {
    int i2c_sda;    // I2C数据线
    int i2c_scl;    // I2C时钟线
    int i2c_addr;   // I2C地址 (可选，默认0x3C)
} ssd1306_i2c_config_t;

/* ========== 公共接口 ========== */

/**
 * @brief 初始化SSD1306驱动
 * @param type 屏幕型号 (DISPLAY_TYPE_OLED_091_SSD1306 或 DISPLAY_TYPE_OLED_096_SSD1306)
 * @param pins 引脚配置，NULL则使用默认配置
 * @return ESP_OK成功，其他值失败
 */
esp_err_t ssd1306_oled_init(display_type_t type, const display_pin_config_t *pins);

/**
 * @brief 反初始化SSD1306驱动
 * @return ESP_OK成功，其他值失败
 */
esp_err_t ssd1306_oled_deinit(void);

/**
 * @brief LVGL flush回调函数
 * @param x1 区域左边界
 * @param y1 区域上边界
 * @param x2 区域右边界
 * @param y2 区域下边界
 * @param color_p 颜色数据指针 (单色格式)
 */
void ssd1306_oled_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint8_t *color_p);

/**
 * @brief LVGL set_px回调函数（单色屏幕需要）
 * @param x X坐标
 * @param y Y坐标
 * @param buf 缓冲区指针
 * @param color 颜色值
 */
void ssd1306_oled_set_px(int32_t x, int32_t y, uint8_t *buf, uint32_t color);

/**
 * @brief LVGL rounder回调函数（单色屏幕需要）
 * @param x1 左边界指针
 * @param y1 上边界指针
 * @param x2 右边界指针
 * @param y2 下边界指针
 */
void ssd1306_oled_rounder(int32_t *x1, int32_t *y1, int32_t *x2, int32_t *y2);

/**
 * @brief 获取屏幕宽度
 * @return 屏幕宽度 (像素)
 */
int ssd1306_oled_get_width(void);

/**
 * @brief 获取屏幕高度
 * @return 屏幕高度 (像素)
 */
int ssd1306_oled_get_height(void);

/**
 * @brief 判断是否为单色屏幕
 * @return true (SSD1306为单色屏幕)
 */
bool ssd1306_oled_is_monochrome(void);

/**
 * @brief 绘制单个像素
 * @param x X坐标
 * @param y Y坐标
 * @param color 颜色值 (0或1)
 */
void ssd1306_oled_draw_pixel(int16_t x, int16_t y, uint16_t color);

/**
 * @brief 填充矩形区域
 * @param x1 左上角X坐标
 * @param y1 左上角Y坐标
 * @param x2 右下角X坐标
 * @param y2 右下角Y坐标
 * @param color 颜色值 (0或1)
 */
void ssd1306_oled_fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);

/**
 * @brief 设置I2C引脚配置
 * @param sda I2C数据引脚
 * @param scl I2C时钟引脚
 * @param addr I2C地址 (可选，默认0x3C)
 * @return ESP_OK成功，其他值失败
 */
esp_err_t ssd1306_oled_set_i2c_pins(int sda, int scl, int addr);

/**
 * @brief 获取SSD1306驱动信息
 * @return 驱动信息结构指针
 */
const display_driver_t *ssd1306_oled_get_driver(void);

/**
 * @brief 根据屏幕型号获取对应的驱动
 * @param type 屏幕型号
 * @return 驱动信息结构指针，无效型号返回NULL
 */
const display_driver_t *ssd1306_oled_get_driver_by_type(display_type_t type);

/**
 * @brief 导出SSD1306 0.91寸驱动结构体（用于驱动注册表）
 */
extern const display_driver_t ssd1306_driver_091;

/**
 * @brief 导出SSD1306 0.96寸驱动结构体（用于驱动注册表）
 */
extern const display_driver_t ssd1306_driver_096;

#endif // SSD1306_OLED_H
