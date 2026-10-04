#include "ili9341_tft.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char *TAG = "ILI9341";

static spi_device_handle_t spi_handle = NULL;
static display_type_t current_type = DISPLAY_TYPE_NONE;
static int current_width = ILI9341_HOR_WIDTH;
static int current_height = ILI9341_HOR_HEIGHT;
static gpio_num_t bl_pin = -1;
static gpio_num_t dc_pin = -1;
static gpio_num_t cs_pin = -1;
static gpio_num_t rst_pin = -1;

static bool double_buffer_enabled = false;
static uint8_t *buffer_a = NULL;
static uint8_t *buffer_b = NULL;
static uint8_t *active_buffer = NULL;

static display_fps_stats_t fps_stats = {0};

/* 红色像素数据（RGB565格式，红色=0xF800）*/
static const uint8_t ili9341_red_pixels[] = {
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
    0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00,
};

typedef struct {
    uint8_t dc_level;
} ili9341_spi_ctx_t;

static void ili9341_spi_pre_cb(spi_transaction_t *t)
{
    ili9341_spi_ctx_t *ctx = (ili9341_spi_ctx_t *)t->user;
    if (ctx) {
        gpio_set_level(dc_pin, ctx->dc_level);
    }
}

static inline void ili9341_send_cmd(uint8_t cmd)
{
    ili9341_spi_ctx_t ctx = {.dc_level = 0};
    
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
        .user = &ctx,
    };
    spi_device_transmit(spi_handle, &t);
}

static inline void ili9341_send_data(uint8_t data)
{
    ili9341_spi_ctx_t ctx = {.dc_level = 1};
    
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
        .user = &ctx,
    };
    spi_device_transmit(spi_handle, &t);
}

static inline void ili9341_send_data16(uint16_t data)
{
    ili9341_spi_ctx_t ctx = {.dc_level = 1};
    uint8_t bytes[2] = {data >> 8, data & 0xFF};
    
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = bytes,
        .user = &ctx,
    };
    spi_device_transmit(spi_handle, &t);
}

static void ili9341_set_window(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    ili9341_send_cmd(0x2A);
    ili9341_send_data(x1 >> 8);
    ili9341_send_data(x1 & 0xFF);
    ili9341_send_data(x2 >> 8);
    ili9341_send_data(x2 & 0xFF);
    
    ili9341_send_cmd(0x2B);
    ili9341_send_data(y1 >> 8);
    ili9341_send_data(y1 & 0xFF);
    ili9341_send_data(y2 >> 8);
    ili9341_send_data(y2 & 0xFF);
    
    ili9341_send_cmd(0x2C);
}

static void ili9341_madctl_set(uint8_t rotation)
{
    ili9341_send_cmd(0x36);
    ili9341_send_data(rotation);
}

static const display_params_t ili9341_params_hor = {
    .type = DISPLAY_TYPE_TFT_24_ILI9341,
    .name = "ILI9341_TFT_24_HOR",
    .width = ILI9341_HOR_WIDTH,
    .height = ILI9341_HOR_HEIGHT,
    .is_monochrome = false,
    .color_depth = 16,
    .spi_freq = CONFIG_DISPLAY_SPI_FREQ * 1000000,
    .support_lvgl = true,
};

static const display_params_t ili9341_params_vert = {
    .type = DISPLAY_TYPE_TFT_24_ILI9341_VERT,
    .name = "ILI9341_TFT_24_VERT",
    .width = ILI9341_VERT_WIDTH,
    .height = ILI9341_VERT_HEIGHT,
    .is_monochrome = false,
    .color_depth = 16,
    .spi_freq = CONFIG_DISPLAY_SPI_FREQ * 1000000,
    .support_lvgl = true,
};

esp_err_t ili9341_tft_init(display_type_t type, const display_pin_config_t *pins)
{
    ESP_LOGI(TAG, "Initializing ILI9341 TFT driver v1.4.0");
    
    const display_pin_config_t *use_pins = pins;
    if (use_pins == NULL) {
        static const display_pin_config_t default_pins = ILI9341_DEFAULT_PINS;
        use_pins = &default_pins;
    }
    
    if (type == DISPLAY_TYPE_TFT_24_ILI9341) {
        current_width = ILI9341_HOR_WIDTH;
        current_height = ILI9341_HOR_HEIGHT;
        current_type = DISPLAY_TYPE_TFT_24_ILI9341;
    } else if (type == DISPLAY_TYPE_TFT_24_ILI9341_VERT) {
        current_width = ILI9341_VERT_WIDTH;
        current_height = ILI9341_VERT_HEIGHT;
        current_type = DISPLAY_TYPE_TFT_24_ILI9341_VERT;
    } else {
        ESP_LOGE(TAG, "Unsupported display type: %d", type);
        return ESP_ERR_NOT_SUPPORTED;
    }
    
    bl_pin = use_pins->bl;
    dc_pin = use_pins->dc;
    cs_pin = use_pins->cs;
    rst_pin = use_pins->rst;
    
    gpio_set_direction(dc_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(dc_pin, 0);
    
    if (rst_pin >= 0) {
        gpio_set_direction(rst_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(rst_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(rst_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    spi_bus_config_t buscfg = {
        .mosi_io_num = use_pins->mosi,
        .miso_io_num = use_pins->miso,
        .sclk_io_num = use_pins->sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = current_width * current_height * 2,
    };
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = CONFIG_DISPLAY_SPI_FREQ * 1000000,
        .mode = 0,
        .spics_io_num = cs_pin,
        .queue_size = 10,
        .pre_cb = ili9341_spi_pre_cb,
        .post_cb = NULL,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        spi_bus_free(SPI2_HOST);
        return ret;
    }
    
    for (int i = 0; i < 50; i++) {
        vTaskDelay(pdMS_TO_TICKS(1));
        taskYIELD();
    }
    
    ili9341_send_cmd(0x11);
    
    for (int i = 0; i < 120; i++) {
        vTaskDelay(pdMS_TO_TICKS(1));
        taskYIELD();
    }
    
    ili9341_send_cmd(0x3A);
    ili9341_send_data(0x55);
    taskYIELD();
    
    ili9341_send_cmd(0xB2);
    ili9341_send_data(0x05);
    ili9341_send_data(0x0C);
    ili9341_send_data(0x0C);
    ili9341_send_data(0x00);
    ili9341_send_data(0x33);
    ili9341_send_data(0x33);
    taskYIELD();
    
    ili9341_send_cmd(0xB7);
    ili9341_send_data(0x75);
    taskYIELD();
    
    ili9341_send_cmd(0xBB);
    ili9341_send_data(0x1C);
    taskYIELD();
    
    ili9341_send_cmd(0xC0);
    ili9341_send_data(0x2C);
    taskYIELD();
    
    ili9341_send_cmd(0xC2);
    ili9341_send_data(0x01);
    taskYIELD();
    
    ili9341_send_cmd(0xC3);
    ili9341_send_data(0x0F);
    taskYIELD();
    
    ili9341_send_cmd(0xC4);
    ili9341_send_data(0x20);
    taskYIELD();
    
    ili9341_send_cmd(0xC6);
    ili9341_send_data(0x01);
    taskYIELD();
    
    ili9341_send_cmd(0xD0);
    ili9341_send_data(0xA4);
    ili9341_send_data(0xA1);
    taskYIELD();
    
    ili9341_send_cmd(0xE0);
    ili9341_send_data(0xD0);
    ili9341_send_data(0x04);
    ili9341_send_data(0x0D);
    ili9341_send_data(0x11);
    ili9341_send_data(0x13);
    ili9341_send_data(0x2B);
    ili9341_send_data(0x3F);
    ili9341_send_data(0x54);
    ili9341_send_data(0x4C);
    ili9341_send_data(0x18);
    ili9341_send_data(0x0D);
    ili9341_send_data(0x0B);
    ili9341_send_data(0x1F);
    ili9341_send_data(0x23);
    taskYIELD();
    
    ili9341_send_cmd(0xE1);
    ili9341_send_data(0xD0);
    ili9341_send_data(0x04);
    ili9341_send_data(0x0C);
    ili9341_send_data(0x11);
    ili9341_send_data(0x13);
    ili9341_send_data(0x2C);
    ili9341_send_data(0x3F);
    ili9341_send_data(0x44);
    ili9341_send_data(0x51);
    ili9341_send_data(0x2F);
    ili9341_send_data(0x1F);
    ili9341_send_data(0x1F);
    ili9341_send_data(0x20);
    ili9341_send_data(0x23);
    taskYIELD();
    
    uint8_t rotation = (type == DISPLAY_TYPE_TFT_24_ILI9341) ? ILI9341_ROTATE_90 : ILI9341_ROTATE_0;
    // rotation &= ~0x08;  // RGB 颜色顺序（默认）
    ili9341_madctl_set(rotation);
    
    ili9341_send_cmd(0x21);
    
    ili9341_send_cmd(0x29);
    
    if (bl_pin >= 0) {
        gpio_set_direction(bl_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(bl_pin, 1);
        ESP_LOGI(TAG, "Backlight enabled on pin %d", bl_pin);
    }
    
    /* 强制填充屏幕为红色，验证显示是否正常 */
    ESP_LOGI(TAG, "Filling screen with red...");
    ili9341_set_window(0, 0, current_width - 1, current_height - 1);
    ili9341_send_cmd(0x2C);
    
    ili9341_spi_ctx_t ctx = {.dc_level = 1};
    int total_pixels = current_width * current_height;
    int chunk_size = current_width * 20;
    
    for (int i = 0; i < total_pixels; i += chunk_size) {
        int remaining = total_pixels - i;
        int send_size = (remaining < chunk_size) ? remaining : chunk_size;
        
        spi_transaction_t t = {
            .length = send_size * 16,
            .tx_buffer = ili9341_red_pixels,
            .user = &ctx,
        };
        spi_device_transmit(spi_handle, &t);
    }
    ESP_LOGI(TAG, "Screen filled with red");
    
    ESP_LOGI(TAG, "ILI9341 initialized: %s (%dx%d), DMA enabled", 
             (type == DISPLAY_TYPE_TFT_24_ILI9341) ? "横屏" : "竖屏",
             current_width, current_height);
    
    return ESP_OK;
}

esp_err_t ili9341_tft_deinit(void)
{
    if (buffer_a) {
        free(buffer_a);
        buffer_a = NULL;
    }
    if (buffer_b) {
        free(buffer_b);
        buffer_b = NULL;
    }
    active_buffer = NULL;
    
    if (spi_handle) {
        spi_bus_remove_device(spi_handle);
        spi_handle = NULL;
    }
    spi_bus_free(SPI2_HOST);
    
    ESP_LOGI(TAG, "ILI9341 deinitialized");
    return ESP_OK;
}

void ili9341_tft_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint8_t *color_p)
{
    ESP_LOGI(TAG, "Flush start: x1=%d, y1=%d, x2=%d, y2=%d", x1, y1, x2, y2);
    
    if (spi_handle == NULL) {
        ESP_LOGE(TAG, "SPI handle is NULL");
        return;
    }
    
    uint32_t start_time = esp_timer_get_time();
    
    ESP_LOGI(TAG, "Setting window...");
    ili9341_set_window(x1, y1, x2, y2);
    ESP_LOGI(TAG, "Window set");
    
    ili9341_spi_ctx_t ctx = {.dc_level = 1};
    
    int len = (x2 - x1 + 1) * (y2 - y1 + 1) * 2;
    int chunk_size = current_width * 2 * 20;
    
    ESP_LOGI(TAG, "Flush len=%d, chunk_size=%d", len, chunk_size);
    
    for (int i = 0; i < len; i += chunk_size) {
        int remaining = len - i;
        int send_size = (remaining < chunk_size) ? remaining : chunk_size;
        
        spi_transaction_t t = {
            .length = send_size * 8,
            .tx_buffer = color_p + i,
            .user = &ctx,
        };
        spi_device_transmit(spi_handle, &t);
    }
    
    uint32_t end_time = esp_timer_get_time();
    uint32_t flush_time_ms = (end_time - start_time) / 1000;
    
    fps_stats.frame_count++;
    fps_stats.total_flush_time_ms += flush_time_ms;
    if (flush_time_ms > fps_stats.max_flush_time_ms) {
        fps_stats.max_flush_time_ms = flush_time_ms;
    }
    if (fps_stats.min_flush_time_ms == 0 || flush_time_ms < fps_stats.min_flush_time_ms) {
        fps_stats.min_flush_time_ms = flush_time_ms;
    }
    
    uint32_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - fps_stats.last_time_ms >= 1000) {
        fps_stats.fps = (float)fps_stats.frame_count * 1000.0f / (now_ms - fps_stats.last_time_ms);
        fps_stats.last_time_ms = now_ms;
        fps_stats.frame_count = 0;
    }
}

int ili9341_tft_get_width(void)
{
    return current_width;
}

int ili9341_tft_get_height(void)
{
    return current_height;
}

bool ili9341_tft_is_monochrome(void)
{
    return false;
}

void ili9341_tft_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (spi_handle == NULL) return;
    
    ili9341_set_window(x, y, x, y);
    ili9341_send_data16(color);
}

void ili9341_tft_fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    if (spi_handle == NULL) return;
    
    ili9341_set_window(x1, y1, x2, y2);
    
    uint8_t bytes[2] = {color >> 8, color & 0xFF};
    int count = (x2 - x1 + 1) * (y2 - y1 + 1);
    
    ili9341_spi_ctx_t ctx = {.dc_level = 1};
    
    for (int i = 0; i < count; i++) {
        spi_transaction_t t = {
            .length = 16,
            .tx_buffer = bytes,
            .user = &ctx,
        };
        spi_device_transmit(spi_handle, &t);
    }
}

esp_err_t ili9341_tft_set_backlight(uint8_t brightness)
{
    if (bl_pin < 0) return ESP_ERR_NOT_SUPPORTED;
    
    gpio_set_level(bl_pin, brightness > 0 ? 1 : 0);
    return ESP_OK;
}

esp_err_t ili9341_tft_set_backlight_pwm(uint8_t brightness, uint32_t freq_hz)
{
    if (bl_pin < 0) return ESP_ERR_NOT_SUPPORTED;
    
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);
    
    ledc_channel_config_t channel_conf = {
        .gpio_num = bl_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = brightness * 255 / 100,
        .hpoint = 0,
    };
    ledc_channel_config(&channel_conf);
    
    return ESP_OK;
}

display_type_t ili9341_tft_auto_detect(const display_pin_config_t *pins)
{
    ESP_LOGI(TAG, "Auto detecting ILI9341...");
    
    if (!pins || pins->sclk < 0 || pins->mosi < 0 || pins->cs < 0) {
        return DISPLAY_TYPE_NONE;
    }
    
    spi_bus_config_t buscfg = {
        .mosi_io_num = pins->mosi,
        .miso_io_num = pins->miso,
        .sclk_io_num = pins->sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64,
    };
    
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = pins->cs,
        .queue_size = 1,
    };
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) return DISPLAY_TYPE_NONE;
    
    spi_device_handle_t handle = NULL;
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &handle);
    if (ret != ESP_OK) {
        spi_bus_free(SPI2_HOST);
        return DISPLAY_TYPE_NONE;
    }
    
    gpio_set_direction(pins->dc, GPIO_MODE_OUTPUT);
    gpio_set_level(pins->dc, 0);
    
    uint8_t cmd = 0x04;
    uint8_t data = 0;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    
    gpio_set_level(pins->dc, 0);
    ret = spi_device_transmit(handle, &t);
    if (ret != ESP_OK) {
        spi_bus_remove_device(handle);
        spi_bus_free(SPI2_HOST);
        return DISPLAY_TYPE_NONE;
    }
    
    t.length = 8;
    t.rx_buffer = &data;
    t.tx_buffer = NULL;
    gpio_set_level(pins->dc, 1);
    ret = spi_device_transmit(handle, &t);
    
    spi_bus_remove_device(handle);
    spi_bus_free(SPI2_HOST);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ILI9341 detected, ID: 0x%02X", data);
        return DISPLAY_TYPE_TFT_24_ILI9341;
    }
    
    return DISPLAY_TYPE_NONE;
}

void ili9341_tft_get_fps_stats(display_fps_stats_t *stats)
{
    if (stats) {
        *stats = fps_stats;
    }
}

void ili9341_tft_reset_fps_stats(void)
{
    memset(&fps_stats, 0, sizeof(display_fps_stats_t));
    fps_stats.last_time_ms = esp_timer_get_time() / 1000;
}

bool ili9341_tft_is_double_buffer_enabled(void)
{
    return double_buffer_enabled;
}

esp_err_t ili9341_tft_enable_double_buffer(bool enable)
{
    if (enable == double_buffer_enabled) {
        return ESP_OK;
    }
    
    if (enable) {
        size_t buf_size = current_width * current_height * 2;
        buffer_a = malloc(buf_size);
        buffer_b = malloc(buf_size);
        if (!buffer_a || !buffer_b) {
            if (buffer_a) free(buffer_a);
            if (buffer_b) free(buffer_b);
            return ESP_ERR_NO_MEM;
        }
        memset(buffer_a, 0, buf_size);
        memset(buffer_b, 0, buf_size);
        active_buffer = buffer_a;
        double_buffer_enabled = true;
        ESP_LOGI(TAG, "Double buffer enabled, buffer size: %u bytes", buf_size);
    } else {
        if (buffer_a) {
            free(buffer_a);
            buffer_a = NULL;
        }
        if (buffer_b) {
            free(buffer_b);
            buffer_b = NULL;
        }
        active_buffer = NULL;
        double_buffer_enabled = false;
        ESP_LOGI(TAG, "Double buffer disabled");
    }
    
    return ESP_OK;
}

static const display_driver_ops_t ili9341_ops = {
    .init = ili9341_tft_init,
    .deinit = ili9341_tft_deinit,
    .flush = ili9341_tft_flush,
    .rounder = NULL,
    .set_px = NULL,
    .get_width = ili9341_tft_get_width,
    .get_height = ili9341_tft_get_height,
    .is_monochrome = ili9341_tft_is_monochrome,
    .draw_pixel = ili9341_tft_draw_pixel,
    .fill = ili9341_tft_fill,
    .set_backlight = ili9341_tft_set_backlight,
    .set_backlight_pwm = ili9341_tft_set_backlight_pwm,
    .auto_detect = ili9341_tft_auto_detect,
    .get_fps_stats = ili9341_tft_get_fps_stats,
    .reset_fps_stats = ili9341_tft_reset_fps_stats,
    .is_double_buffer_enabled = ili9341_tft_is_double_buffer_enabled,
    .enable_double_buffer = ili9341_tft_enable_double_buffer,
};

const display_driver_t ili9341_driver_hor = {
    .type = DISPLAY_TYPE_TFT_24_ILI9341,
    .driver_name = "ILI9341_TFT_24_HOR",
    .version = ILI9341_DRIVER_VERSION,
    .params = &ili9341_params_hor,
    .ops = &ili9341_ops,
    .default_pins = &(const display_pin_config_t)ILI9341_DEFAULT_PINS,
};

const display_driver_t ili9341_driver_vert = {
    .type = DISPLAY_TYPE_TFT_24_ILI9341_VERT,
    .driver_name = "ILI9341_TFT_24_VERT",
    .version = ILI9341_DRIVER_VERSION,
    .params = &ili9341_params_vert,
    .ops = &ili9341_ops,
    .default_pins = &(const display_pin_config_t)ILI9341_DEFAULT_PINS,
};

const display_driver_t *ili9341_tft_get_driver(void)
{
    return &ili9341_driver_hor;
}

const display_driver_t *ili9341_tft_get_driver_by_type(display_type_t type)
{
    switch (type) {
        case DISPLAY_TYPE_TFT_24_ILI9341:
            return &ili9341_driver_hor;
        case DISPLAY_TYPE_TFT_24_ILI9341_VERT:
            return &ili9341_driver_vert;
        default:
            return NULL;
    }
}
