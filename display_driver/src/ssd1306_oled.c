#include "ssd1306_oled.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_timer.h"

static const char *TAG = "SSD1306";

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t i2c_dev = NULL;
static display_type_t current_type = DISPLAY_TYPE_NONE;
static int current_width = SSD1306_091_WIDTH;
static int current_height = SSD1306_091_HEIGHT;
static int i2c_addr = SSD1306_I2C_ADDR;
static uint8_t *oled_buffer = NULL;

static bool double_buffer_enabled = false;
static uint8_t *buffer_a = NULL;
static uint8_t *buffer_b = NULL;
static uint8_t *active_buffer = NULL;

static display_fps_stats_t fps_stats = {0};

static const display_params_t ssd1306_params_091 = {
    .type = DISPLAY_TYPE_OLED_091_SSD1306,
    .name = "0.91寸 OLED SSD1306 (128x32)",
    .width = SSD1306_091_WIDTH,
    .height = SSD1306_091_HEIGHT,
    .is_monochrome = true,
    .color_depth = SSD1306_COLOR_DEPTH,
    .spi_freq = SSD1306_I2C_FREQ,
    .support_lvgl = true,
};

static const display_params_t ssd1306_params_096 = {
    .type = DISPLAY_TYPE_OLED_096_SSD1306,
    .name = "0.96寸 OLED SSD1306 (128x64)",
    .width = SSD1306_096_WIDTH,
    .height = SSD1306_096_HEIGHT,
    .is_monochrome = true,
    .color_depth = SSD1306_COLOR_DEPTH,
    .spi_freq = SSD1306_I2C_FREQ,
    .support_lvgl = true,
};

static const display_pin_config_t default_pins = SSD1306_DEFAULT_PINS;

static esp_err_t ssd1306_write_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    esp_err_t ret = i2c_master_transmit(i2c_dev, buf, 2, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write cmd 0x%02X failed: %s", cmd, esp_err_to_name(ret));
    } else {
        ESP_LOGV(TAG, "I2C write cmd 0x%02X succeeded", cmd);
    }
    return ret;
}

static esp_err_t ssd1306_write_data(uint8_t data)
{
    uint8_t buf[2] = {0x40, data};
    esp_err_t ret = i2c_master_transmit(i2c_dev, buf, 2, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write data 0x%02X failed: %s", data, esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t ssd1306_write_data_buf(const uint8_t *data, int len)
{
    uint8_t *buf = malloc(len + 1);
    if (buf == NULL) return ESP_ERR_NO_MEM;
    
    buf[0] = 0x40;
    memcpy(buf + 1, data, len);
    
    esp_err_t ret = i2c_master_transmit(i2c_dev, buf, len + 1, 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write data buf (len=%d) failed: %s", len, esp_err_to_name(ret));
    }
    free(buf);
    return ret;
}

static void ssd1306_init_sequence(void)
{
    ESP_LOGI(TAG, "SSD1306 init sequence starting");
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ssd1306_write_cmd(0xAE);
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ssd1306_write_cmd(0xD5);
    ssd1306_write_cmd(0x80);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0xA8);
    if (current_height == 32) {
        ssd1306_write_cmd(31);
    } else {
        ssd1306_write_cmd(63);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0xD3);
    ssd1306_write_cmd(0x00);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0x40);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0x8D);
    ssd1306_write_cmd(0x14);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ssd1306_write_cmd(0x20);
    ssd1306_write_cmd(0x00);  /* 水平地址模式（列自动递增，页自动递增） */
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0xA1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0xC8);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0xDA);
    if (current_height == 32) {
        ssd1306_write_cmd(0x02);
    } else {
        ssd1306_write_cmd(0x12);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0x81);
    ssd1306_write_cmd(0xFF);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0xD9);
    ssd1306_write_cmd(0xF1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0xDB);
    ssd1306_write_cmd(0x40);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0xA4);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0xA6);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    ssd1306_write_cmd(0xAF);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "SSD1306 init sequence completed");
}

static bool ssd1306_test_i2c_address(uint8_t addr)
{
    i2c_device_config_t test_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = CONFIG_DISPLAY_I2C_FREQ * 1000,
    };
    i2c_master_dev_handle_t test_dev;
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &test_config, &test_dev);
    if (ret != ESP_OK) {
        return false;
    }
    
    uint8_t cmd = 0x00;
    ret = i2c_master_transmit(test_dev, &cmd, 1, 100);
    i2c_master_bus_rm_device(test_dev);
    
    return (ret == ESP_OK);
}

esp_err_t ssd1306_oled_init(display_type_t type, const display_pin_config_t *pins)
{
    ESP_LOGI(TAG, "Initializing SSD1306 OLED driver v1.1.0");
    
    if (type != DISPLAY_TYPE_OLED_091_SSD1306 && type != DISPLAY_TYPE_OLED_096_SSD1306) {
        ESP_LOGE(TAG, "Invalid display type: %d", type);
        return ESP_ERR_INVALID_ARG;
    }
    
    const display_pin_config_t *use_pins = (pins != NULL) ? pins : &default_pins;
    
    if (type == DISPLAY_TYPE_OLED_091_SSD1306) {
        current_width = SSD1306_091_WIDTH;
        current_height = SSD1306_091_HEIGHT;
    } else {
        current_width = SSD1306_096_WIDTH;
        current_height = SSD1306_096_HEIGHT;
    }
    
    current_type = type;
    
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = use_pins->i2c_scl,
        .sda_io_num = use_pins->i2c_sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Scanning I2C bus for SSD1306 devices...");
    bool found = false;
    
    if (ssd1306_test_i2c_address(0x3C)) {
        ESP_LOGI(TAG, "SSD1306 found at address 0x3C");
        i2c_addr = 0x3C;
        found = true;
    }
    
    if (!found && ssd1306_test_i2c_address(0x3D)) {
        ESP_LOGI(TAG, "SSD1306 found at address 0x3D");
        i2c_addr = 0x3D;
        found = true;
    }
    
    if (!found) {
        ESP_LOGE(TAG, "No SSD1306 device found at 0x3C or 0x3D!");
        ESP_LOGI(TAG, "Scanning all I2C addresses...");
        for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
            if (ssd1306_test_i2c_address(addr)) {
                ESP_LOGI(TAG, "I2C device found at address 0x%02X", addr);
            }
        }
        i2c_del_master_bus(i2c_bus);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Using I2C address 0x%02X", i2c_addr);
    
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = i2c_addr,
        .scl_speed_hz = CONFIG_DISPLAY_I2C_FREQ * 1000,
    };
    
    ret = i2c_master_bus_add_device(i2c_bus, &dev_config, &i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C device add failed: %s", esp_err_to_name(ret));
        i2c_del_master_bus(i2c_bus);
        return ret;
    }
    
    oled_buffer = calloc(current_width * current_height / 8, sizeof(uint8_t));
    if (oled_buffer == NULL) {
        ESP_LOGE(TAG, "Buffer allocation failed");
        i2c_master_bus_rm_device(i2c_dev);
        i2c_del_master_bus(i2c_bus);
        return ESP_ERR_NO_MEM;
    }
    
    ssd1306_init_sequence();
    
    ssd1306_oled_fill(0, 0, current_width - 1, current_height - 1, 0);
    
    ESP_LOGI(TAG, "SSD1306 initialized: %s (%dx%d), I2C addr=0x%02X", 
             (type == DISPLAY_TYPE_OLED_091_SSD1306) ? "0.91寸" : "0.96寸",
             current_width, current_height, i2c_addr);
    
    return ESP_OK;
}

esp_err_t ssd1306_oled_deinit(void)
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
    
    if (oled_buffer) {
        free(oled_buffer);
        oled_buffer = NULL;
    }
    if (i2c_dev) {
        i2c_master_bus_rm_device(i2c_dev);
        i2c_dev = NULL;
    }
    if (i2c_bus) {
        i2c_del_master_bus(i2c_bus);
        i2c_bus = NULL;
    }
    current_type = DISPLAY_TYPE_NONE;
    return ESP_OK;
}

void ssd1306_oled_flush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint8_t *color_p)
{
    uint32_t start_time = esp_timer_get_time();

    ESP_LOGD(TAG, "ssd1306_oled_flush: x1=%d y1=%d x2=%d y2=%d, buf=%p",
             x1, y1, x2, y2, color_p);

    if (i2c_dev == NULL || oled_buffer == NULL) {
        ESP_LOGW(TAG, "flush skipped: i2c_dev=%p, oled_buffer=%p", i2c_dev, oled_buffer);
        return;
    }

    int width = x2 - x1 + 1;
    int height = y2 - y1 + 1;
    int page_start = y1 / 8;
    int page_end = y2 / 8;

    /* I1 格式: LVGL 在缓冲区前预留 8 字节调色板，需跳过 */
    const uint8_t *px_data = color_p + 8;

    /* I1 格式 stride: 用 LVGL API 计算（考虑对齐） */
    int stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_I1);

    /* 将 I1 格式转换为 SSD1306 页格式，存入 oled_buffer
     * 注意：LVGL 9.x I1 格式默认调色板 索引0=白, 索引1=黑
     * SSD1306: bit=1 亮, bit=0 暗
     * 因此 I1 bit=0(白) -> SSD1306 bit=1(亮), I1 bit=1(黑) -> SSD1306 bit=0(暗) */
    for (int page = page_start; page <= page_end; page++) {
        uint8_t *page_buf = oled_buffer + page * current_width + x1;

        for (int x = 0; x < width; x++) {
            uint8_t byte_val = 0;

            for (int bit = 0; bit < 8; bit++) {
                int oled_y = page * 8 + bit;
                int buf_y = oled_y - y1;

                if (buf_y >= 0 && buf_y < height) {
                    uint8_t i1_byte = px_data[stride * buf_y + (x / 8)];
                    uint8_t i1_bit = 7 - (x % 8);
                    /* I1 bit=0(白) -> SSD1306 亮 */
                    if (!(i1_byte & (1 << i1_bit))) {
                        byte_val |= (1 << bit);
                    }
                }
            }

            page_buf[x] = byte_val;
        }
    }

    /* 在水平地址模式下，必须一次性连续写入所有页数据
     * SSD1306 自动递增列地址，溢出时自动翻页
     * 分块写入以兼容不同 I2C DMA 缓冲区大小 */
    int total_bytes = (page_end - page_start + 1) * width;
    uint8_t *data_start = oled_buffer + page_start * current_width + x1;

    ssd1306_write_cmd(0x21);
    ssd1306_write_cmd(x1);
    ssd1306_write_cmd(x2);

    ssd1306_write_cmd(0x22);
    ssd1306_write_cmd(page_start);
    ssd1306_write_cmd(page_end);

    /* 分块写入，每块最多 256 字节 */
    const int CHUNK_SIZE = 256;
    int offset = 0;
    while (offset < total_bytes) {
        int chunk = (total_bytes - offset > CHUNK_SIZE) ? CHUNK_SIZE : (total_bytes - offset);
        ssd1306_write_data_buf(data_start + offset, chunk);
        offset += chunk;
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

    ESP_LOGD(TAG, "ssd1306_oled_flush: completed (%lu bytes, %lums)",
             (unsigned long)total_bytes, (unsigned long)flush_time_ms);
}

void ssd1306_oled_set_px(int32_t x, int32_t y, uint8_t *buf, uint32_t color)
{
    int bytes_per_row = current_width;
    
    uint8_t *byte = buf + (y / 8) * bytes_per_row + x;
    
    if (color) {
        *byte |= (1 << (y % 8));
    } else {
        *byte &= ~(1 << (y % 8));
    }
}

void ssd1306_oled_rounder(int32_t *x1, int32_t *y1, int32_t *x2, int32_t *y2)
{
    /* I1 格式需要 x 对齐到 8 像素（1字节）边界 */
    *x1 = (*x1 / 8) * 8;
    *x2 = ((*x2 + 7) / 8) * 8 - 1;
    /* y 对齐到 8 像素（1页）边界 */
    *y1 = (*y1 / 8) * 8;
    *y2 = ((*y2 + 7) / 8) * 8 - 1;
}

int ssd1306_oled_get_width(void)
{
    return current_width;
}

int ssd1306_oled_get_height(void)
{
    return current_height;
}

bool ssd1306_oled_is_monochrome(void)
{
    return true;
}

void ssd1306_oled_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || x >= current_width || y < 0 || y >= current_height) return;
    if (oled_buffer == NULL) return;
    
    uint16_t byte_idx = (y / 8) * current_width + x;
    uint8_t bit_idx = y % 8;
    
    if (color) {
        oled_buffer[byte_idx] |= (1 << bit_idx);
    } else {
        oled_buffer[byte_idx] &= ~(1 << bit_idx);
    }
    
    ssd1306_write_cmd(0x21);
    ssd1306_write_cmd(x);
    ssd1306_write_cmd(x);
    
    ssd1306_write_cmd(0x22);
    ssd1306_write_cmd(y / 8);
    ssd1306_write_cmd(y / 8);
    
    ssd1306_write_data(oled_buffer[byte_idx]);
}

void ssd1306_oled_fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= current_width) x2 = current_width - 1;
    if (y2 >= current_height) y2 = current_height - 1;
    
    ssd1306_write_cmd(0x21);
    ssd1306_write_cmd(x1);
    ssd1306_write_cmd(x2);
    
    ssd1306_write_cmd(0x22);
    ssd1306_write_cmd(y1 / 8);
    ssd1306_write_cmd(y2 / 8);
    
    uint8_t fill_data = color ? 0xFF : 0x00;
    int width = x2 - x1 + 1;
    int pages = (y2 / 8) - (y1 / 8) + 1;
    int len = width * pages;
    
    for (int i = 0; i < len; i++) {
        ssd1306_write_data(fill_data);
    }
}

esp_err_t ssd1306_oled_set_backlight(uint8_t brightness)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t ssd1306_oled_set_backlight_pwm(uint8_t brightness, uint32_t freq_hz)
{
    return ESP_ERR_NOT_SUPPORTED;
}

display_type_t ssd1306_oled_auto_detect(const display_pin_config_t *pins)
{
    ESP_LOGI(TAG, "Auto detecting SSD1306...");
    
    if (!pins || pins->i2c_sda < 0 || pins->i2c_scl < 0) {
        return DISPLAY_TYPE_NONE;
    }
    
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = pins->i2c_scl,
        .sda_io_num = pins->i2c_sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    
    i2c_master_bus_handle_t bus;
    esp_err_t ret = i2c_new_master_bus(&bus_config, &bus);
    if (ret != ESP_OK) {
        return DISPLAY_TYPE_NONE;
    }
    
    bool found = false;
    int test_addr = 0;
    
    for (uint8_t addr = 0x3C; addr <= 0x3D; addr++) {
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 100000,
        };
        
        i2c_master_dev_handle_t dev;
        ret = i2c_master_bus_add_device(bus, &dev_config, &dev);
        if (ret == ESP_OK) {
            uint8_t buf[2] = {0x00, 0xAE};
            ret = i2c_master_transmit(dev, buf, 2, 100);
            i2c_master_bus_rm_device(dev);
            
            if (ret == ESP_OK) {
                found = true;
                test_addr = addr;
                break;
            }
        }
    }
    
    i2c_del_master_bus(bus);
    
    if (found) {
        ESP_LOGI(TAG, "SSD1306 detected at 0x%02X", test_addr);
        return DISPLAY_TYPE_OLED_096_SSD1306;
    }
    
    return DISPLAY_TYPE_NONE;
}

void ssd1306_oled_get_fps_stats(display_fps_stats_t *stats)
{
    if (stats) {
        *stats = fps_stats;
    }
}

void ssd1306_oled_reset_fps_stats(void)
{
    memset(&fps_stats, 0, sizeof(display_fps_stats_t));
    fps_stats.last_time_ms = esp_timer_get_time() / 1000;
}

bool ssd1306_oled_is_double_buffer_enabled(void)
{
    return double_buffer_enabled;
}

esp_err_t ssd1306_oled_enable_double_buffer(bool enable)
{
    if (enable == double_buffer_enabled) {
        return ESP_OK;
    }
    
    if (enable) {
        size_t buf_size = current_width * current_height / 8;
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

static const display_driver_ops_t ssd1306_ops = {
    .init = ssd1306_oled_init,
    .deinit = ssd1306_oled_deinit,
    .flush = ssd1306_oled_flush,
    .rounder = ssd1306_oled_rounder,
    .set_px = ssd1306_oled_set_px,
    .get_width = ssd1306_oled_get_width,
    .get_height = ssd1306_oled_get_height,
    .is_monochrome = ssd1306_oled_is_monochrome,
    .draw_pixel = ssd1306_oled_draw_pixel,
    .fill = ssd1306_oled_fill,
    .set_backlight = ssd1306_oled_set_backlight,
    .set_backlight_pwm = ssd1306_oled_set_backlight_pwm,
    .auto_detect = ssd1306_oled_auto_detect,
    .get_fps_stats = ssd1306_oled_get_fps_stats,
    .reset_fps_stats = ssd1306_oled_reset_fps_stats,
    .is_double_buffer_enabled = ssd1306_oled_is_double_buffer_enabled,
    .enable_double_buffer = ssd1306_oled_enable_double_buffer,
};

const display_driver_t ssd1306_driver_091 = {
    .type = DISPLAY_TYPE_OLED_091_SSD1306,
    .driver_name = "SSD1306_OLED_091",
    .version = SSD1306_DRIVER_VERSION,
    .params = &ssd1306_params_091,
    .ops = &ssd1306_ops,
    .default_pins = &default_pins,
};

const display_driver_t ssd1306_driver_096 = {
    .type = DISPLAY_TYPE_OLED_096_SSD1306,
    .driver_name = "SSD1306_OLED_096",
    .version = SSD1306_DRIVER_VERSION,
    .params = &ssd1306_params_096,
    .ops = &ssd1306_ops,
    .default_pins = &default_pins,
};

const display_driver_t *ssd1306_oled_get_driver(void)
{
    if (current_type == DISPLAY_TYPE_OLED_091_SSD1306) {
        return &ssd1306_driver_091;
    } else if (current_type == DISPLAY_TYPE_OLED_096_SSD1306) {
        return &ssd1306_driver_096;
    }
    return NULL;
}

const display_driver_t *ssd1306_oled_get_driver_by_type(display_type_t type)
{
    if (type == DISPLAY_TYPE_OLED_091_SSD1306) {
        return &ssd1306_driver_091;
    } else if (type == DISPLAY_TYPE_OLED_096_SSD1306) {
        return &ssd1306_driver_096;
    }
    return NULL;
}
