#include "display_driver.h"
#include "ili9341_tft.h"
#include "ssd1306_oled.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"

static const char *TAG = "DISP_REG";

static const display_driver_t *current_driver = NULL;

static const display_driver_t *driver_registry[] = {
    &ili9341_driver_hor,
    &ili9341_driver_vert,
    &ssd1306_driver_091,
    &ssd1306_driver_096,
    NULL
};

static bool ili9341_probe(const display_pin_config_t *pins)
{
    if (!pins) return false;
    
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
    if (ret != ESP_OK) return false;
    
    spi_device_handle_t handle = NULL;
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &handle);
    if (ret != ESP_OK) {
        spi_bus_free(SPI2_HOST);
        return false;
    }
    
    if (pins->dc >= 0) {
        gpio_set_direction(pins->dc, GPIO_MODE_OUTPUT);
        gpio_set_level(pins->dc, 0);
    }
    
    uint8_t cmd = 0x04;
    uint8_t data = 0;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    
    if (pins->dc >= 0) {
        gpio_set_level(pins->dc, 0);
    }
    ret = spi_device_transmit(handle, &t);
    if (ret != ESP_OK) {
        spi_bus_remove_device(handle);
        spi_bus_free(SPI2_HOST);
        return false;
    }
    
    t.length = 8;
    t.rx_buffer = &data;
    t.tx_buffer = NULL;
    if (pins->dc >= 0) {
        gpio_set_level(pins->dc, 1);
    }
    ret = spi_device_transmit(handle, &t);
    
    spi_bus_remove_device(handle);
    spi_bus_free(SPI2_HOST);
    
    return (ret == ESP_OK);
}

display_type_t display_auto_detect(const display_pin_config_t *pins)
{
    ESP_LOGI(TAG, "Auto detecting display type...");
    
    if (!pins) {
        ESP_LOGW(TAG, "No pin config provided, cannot auto detect");
        return DISPLAY_TYPE_NONE;
    }
    
    if (pins->sclk >= 0 && pins->mosi >= 0 && pins->cs >= 0) {
        if (ili9341_probe(pins)) {
            ESP_LOGI(TAG, "Detected: ILI9341 TFT");
            return DISPLAY_TYPE_TFT_24_ILI9341;
        }
    }
    
    if (pins->i2c_sda >= 0 && pins->i2c_scl >= 0) {
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
        if (ret == ESP_OK) {
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
                        ESP_LOGI(TAG, "Detected: SSD1306 OLED at 0x%02X", addr);
                        i2c_del_master_bus(bus);
                        return DISPLAY_TYPE_OLED_096_SSD1306;
                    }
                }
            }
            i2c_del_master_bus(bus);
        }
    }
    
    ESP_LOGW(TAG, "No display detected, returning DISPLAY_TYPE_NONE");
    return DISPLAY_TYPE_NONE;
}

const display_driver_t *display_driver_find(display_type_t type)
{
    for (int i = 0; driver_registry[i] != NULL; i++) {
        if (driver_registry[i]->type == type) {
            ESP_LOGI(TAG, "Found driver: %s v%s for type %d", 
                     driver_registry[i]->driver_name,
                     driver_registry[i]->version,
                     type);
            return driver_registry[i];
        }
    }
    ESP_LOGW(TAG, "Driver not found for type: %d", type);
    return NULL;
}

esp_err_t display_driver_init(display_type_t type, const display_pin_config_t *pins)
{
    ESP_LOGI(TAG, "Initializing display driver v%s", DISPLAY_DRIVER_VERSION_STRING);
    
    const display_driver_t *driver = display_driver_find(type);
    if (driver == NULL) {
        ESP_LOGE(TAG, "No driver found for display type %d", type);
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Using driver: %s v%s", driver->driver_name, driver->version);
    ESP_LOGI(TAG, "Screen: %s (%dx%d, %d-bit color)", 
             driver->params->name,
             driver->params->width,
             driver->params->height,
             driver->params->color_depth);
    
    if (!driver->params->support_lvgl) {
        ESP_LOGW(TAG, "Driver %s does not support LVGL", driver->driver_name);
    }
    
    const display_pin_config_t *use_pins = pins;
    if (use_pins == NULL) {
        use_pins = driver->default_pins;
        ESP_LOGI(TAG, "Using default pin configuration");
    }
    
    esp_err_t ret = driver->ops->init(type, use_pins);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Driver init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    current_driver = driver;
    
    ESP_LOGI(TAG, "Display initialized successfully!");
    
    return ESP_OK;
}

esp_err_t display_driver_deinit(void)
{
    if (current_driver == NULL) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing display driver: %s", current_driver->driver_name);
    
    esp_err_t ret = current_driver->ops->deinit();
    current_driver = NULL;
    
    return ret;
}

const display_driver_t *display_driver_get_current(void)
{
    return current_driver;
}

const display_params_t *display_get_params(void)
{
    if (current_driver == NULL) {
        return NULL;
    }
    return current_driver->params;
}

int display_get_width(void)
{
    if (current_driver == NULL || current_driver->ops->get_width == NULL) {
        return 0;
    }
    return current_driver->ops->get_width();
}

int display_get_height(void)
{
    if (current_driver == NULL || current_driver->ops->get_height == NULL) {
        return 0;
    }
    return current_driver->ops->get_height();
}

bool display_is_monochrome(void)
{
    if (current_driver == NULL || current_driver->ops->is_monochrome == NULL) {
        return false;
    }
    return current_driver->ops->is_monochrome();
}

const char *display_driver_get_version(void)
{
    return DISPLAY_DRIVER_VERSION_STRING;
}

void display_get_fps_stats(display_fps_stats_t *stats)
{
    if (current_driver == NULL || current_driver->ops->get_fps_stats == NULL) {
        if (stats) {
            memset(stats, 0, sizeof(display_fps_stats_t));
        }
        return;
    }
    current_driver->ops->get_fps_stats(stats);
}

void display_reset_fps_stats(void)
{
    if (current_driver != NULL && current_driver->ops->reset_fps_stats != NULL) {
        current_driver->ops->reset_fps_stats();
    }
}

esp_err_t display_set_backlight_pwm(uint8_t brightness, uint32_t freq_hz)
{
    if (current_driver == NULL || current_driver->ops->set_backlight_pwm == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return current_driver->ops->set_backlight_pwm(brightness, freq_hz);
}

bool display_is_double_buffer_enabled(void)
{
    if (current_driver == NULL || current_driver->ops->is_double_buffer_enabled == NULL) {
        return false;
    }
    return current_driver->ops->is_double_buffer_enabled();
}

esp_err_t display_enable_double_buffer(bool enable)
{
    if (current_driver == NULL || current_driver->ops->enable_double_buffer == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return current_driver->ops->enable_double_buffer(enable);
}

void display_driver_print_registry(void)
{
    ESP_LOGI(TAG, "=== Display Driver Registry ===");
    ESP_LOGI(TAG, "Core version: %s", DISPLAY_DRIVER_VERSION_STRING);
    
    int count = 0;
    for (int i = 0; driver_registry[i] != NULL; i++) {
        ESP_LOGI(TAG, "[%d] %s v%s - %s (%dx%d)",
                 i,
                 driver_registry[i]->driver_name,
                 driver_registry[i]->version,
                 driver_registry[i]->params->name,
                 driver_registry[i]->params->width,
                 driver_registry[i]->params->height);
        count++;
    }
    
    ESP_LOGI(TAG, "Total registered drivers: %d", count);
}
