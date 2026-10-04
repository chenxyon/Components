#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "esp_err.h"
#include "lvgl.h"

#define DISPLAY_DRIVER_VERSION_MAJOR  2
#define DISPLAY_DRIVER_VERSION_MINOR  2
#define DISPLAY_DRIVER_VERSION_PATCH  0
#define DISPLAY_DRIVER_VERSION_STRING  "2.2.0"

typedef enum {
    DISPLAY_TYPE_NONE = 0,
    DISPLAY_TYPE_OLED_091_SSD1306,
    DISPLAY_TYPE_OLED_096_SSD1306,
    DISPLAY_TYPE_TFT_24_ILI9341,
    DISPLAY_TYPE_TFT_24_ILI9341_VERT,
    DISPLAY_TYPE_TFT_18_ST7789,
    DISPLAY_TYPE_TFT_24_ST7789,
    DISPLAY_TYPE_MAX
} display_type_t;

typedef enum {
    TEXT_TYPE_AUTO = 0,
    TEXT_TYPE_ENGLISH,
    TEXT_TYPE_CHINESE,
    TEXT_TYPE_ICON
} text_type_t;

typedef struct {
    int mosi;
    int miso;
    int sclk;
    int cs;
    int dc;
    int rst;
    int bl;
    int i2c_sda;
    int i2c_scl;
    int i2c_addr;
} display_pin_config_t;

typedef struct {
    display_type_t type;
    const char *name;
    uint16_t width;
    uint16_t height;
    bool is_monochrome;
    uint8_t color_depth;
    uint32_t spi_freq;
    bool support_lvgl;
} display_params_t;

typedef struct {
    uint32_t frame_count;
    uint32_t last_time_ms;
    float fps;
    uint32_t total_flush_time_ms;
    uint32_t max_flush_time_ms;
    uint32_t min_flush_time_ms;
} display_fps_stats_t;

typedef struct {
    esp_err_t (*init)(display_type_t type, const display_pin_config_t *pins);
    esp_err_t (*deinit)(void);
    void (*flush)(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint8_t *color_p);
    void (*rounder)(int32_t *x1, int32_t *y1, int32_t *x2, int32_t *y2);
    void (*set_px)(int32_t x, int32_t y, uint8_t *buf, uint32_t color);
    int (*get_width)(void);
    int (*get_height)(void);
    bool (*is_monochrome)(void);
    void (*draw_pixel)(int16_t x, int16_t y, uint16_t color);
    void (*fill)(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
    esp_err_t (*set_backlight)(uint8_t brightness);
    esp_err_t (*set_backlight_pwm)(uint8_t brightness, uint32_t freq_hz);
    display_type_t (*auto_detect)(const display_pin_config_t *pins);
    void (*get_fps_stats)(display_fps_stats_t *stats);
    void (*reset_fps_stats)(void);
    bool (*is_double_buffer_enabled)(void);
    esp_err_t (*enable_double_buffer)(bool enable);
} display_driver_ops_t;

typedef struct {
    display_type_t type;
    const char *driver_name;
    const char *version;
    const display_params_t *params;
    const display_driver_ops_t *ops;
    const display_pin_config_t *default_pins;
} display_driver_t;

esp_err_t display_driver_init(display_type_t type, const display_pin_config_t *pins);
esp_err_t display_driver_deinit(void);
const display_driver_t *display_driver_get_current(void);
const display_driver_t *display_driver_find(display_type_t type);
const display_params_t *display_get_params(void);
int display_get_width(void);
int display_get_height(void);
bool display_is_monochrome(void);
const char *display_driver_get_version(void);

display_type_t display_auto_detect(const display_pin_config_t *pins);
void display_get_fps_stats(display_fps_stats_t *stats);
void display_reset_fps_stats(void);
esp_err_t display_set_backlight_pwm(uint8_t brightness, uint32_t freq_hz);
bool display_is_double_buffer_enabled(void);
esp_err_t display_enable_double_buffer(bool enable);

text_type_t display_detect_text_type(const char *text);

lv_obj_t* display_create_label(lv_obj_t *parent, text_type_t type, const char *text);
lv_obj_t* display_create_label_fmt(lv_obj_t *parent, text_type_t type, const char *format, ...);
lv_obj_t* display_create_rich_text(lv_obj_t *parent, const char *text);
lv_obj_t* display_create_rich_text_fmt(lv_obj_t *parent, const char *format, ...);

#endif
