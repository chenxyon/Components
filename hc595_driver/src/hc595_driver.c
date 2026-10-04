#include "hc595_driver.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "HC595";

static hc595_config_t s_config;
static uint32_t s_current_output = 0;
static bool s_initialized = false;

static void hc595_shift_out(uint8_t data)
{
    for (int i = 7; i >= 0; i--) {
        gpio_set_level(s_config.ds_pin, (data >> i) & 0x01);
        gpio_set_level(s_config.shcp_pin, 1);
        gpio_set_level(s_config.shcp_pin, 0);
    }
}

static void hc595_latch(void)
{
    gpio_set_level(s_config.stcp_pin, 1);
    gpio_set_level(s_config.stcp_pin, 0);
}

esp_err_t hc595_init(const hc595_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (config->num_chips < 1 || config->num_chips > 4) {
        ESP_LOGE(TAG, "Invalid chip count: %d (1-4)", config->num_chips);
        return ESP_ERR_INVALID_ARG;
    }

    s_config = *config;

    /* 配置输出引脚 */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_config.ds_pin) |
                        (1ULL << s_config.shcp_pin) |
                        (1ULL << s_config.stcp_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始状态: 全低 */
    gpio_set_level(s_config.ds_pin, 0);
    gpio_set_level(s_config.shcp_pin, 0);
    gpio_set_level(s_config.stcp_pin, 0);

    s_current_output = 0;
    s_initialized = true;

    /* 清空输出 */
    hc595_write(0);

    ESP_LOGI(TAG, "74HC595 initialized: DS=%d, SHCP=%d, STCP=%d, chips=%d",
             s_config.ds_pin, s_config.shcp_pin, s_config.stcp_pin, s_config.num_chips);

    return ESP_OK;
}

void hc595_write(uint32_t data)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return;
    }

    for (int i = s_config.num_chips - 1; i >= 0; i--) {
        hc595_shift_out((data >> (i * 8)) & 0xFF);
    }
    hc595_latch();
    s_current_output = data;
}

uint32_t hc595_get_output(void)
{
    return s_current_output;
}

void hc595_set_single(int bit_index)
{
    if (!s_initialized) {
        return;
    }
    hc595_write(1UL << bit_index);
}

void hc595_clear(void)
{
    hc595_write(0);
}
