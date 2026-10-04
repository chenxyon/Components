#include <string.h>
#include "w25qxx_spi.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "freertos/task.h"

static const char *TAG = "w25qxx_spi";

bool w25qxx_spi_init(w25qxx_config_t *config) {
    spi_bus_config_t buscfg = {
        .miso_io_num = config->miso_pin,
        .mosi_io_num = config->mosi_pin,
        .sclk_io_num = config->sclk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8192,
    };

    esp_err_t ret = spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return false;
    }

    int spi_mode = (config->spi_mode == 0 || config->spi_mode == 3) ? config->spi_mode : 3;
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000,
        .mode = spi_mode,
        .spics_io_num = -1,
        .queue_size = 7,
    };
    ESP_LOGI(TAG, "SPI Mode: %d (CPOL=%d, CPHA=%d)", spi_mode, (spi_mode >> 1) & 1, spi_mode & 1);

    ret = spi_bus_add_device(config->spi_host, &devcfg, &config->spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        spi_bus_free(config->spi_host);
        return false;
    }

    gpio_config_t cs_gpio_config = {
        .pin_bit_mask = (1ULL << config->cs_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&cs_gpio_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure CS GPIO: %s", esp_err_to_name(ret));
        spi_bus_remove_device(config->spi_handle);
        config->spi_handle = NULL;
        spi_bus_free(config->spi_host);
        return false;
    }

    w25qxx_spi_cs_deselect(config);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "SPI init (software CS): host=%d, CS=%d, SCLK=%d, MOSI=%d, MISO=%d",
             config->spi_host, config->cs_pin, config->sclk_pin, config->mosi_pin, config->miso_pin);
    return true;
}

void w25qxx_spi_deinit(w25qxx_config_t *config) {
    if (config != NULL && config->spi_handle != NULL) {
        spi_bus_remove_device(config->spi_handle);
        config->spi_handle = NULL;
    }
    if (config != NULL) {
        spi_bus_free(config->spi_host);
    }
}

bool w25qxx_spi_loopback_test(w25qxx_config_t *config) {
    uint8_t test_data[8] = {0x9F, 0xAA, 0x55, 0xFF, 0x00, 0x12, 0x34, 0x56};
    uint8_t rx_data[8] = {0};

    ESP_LOGI(TAG, "=== SPI Loopback Test ===");
    ESP_LOGI(TAG, "TX: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
             test_data[0], test_data[1], test_data[2], test_data[3],
             test_data[4], test_data[5], test_data[6], test_data[7]);

    spi_transaction_t t = {
        .length = 8 * 8,
        .tx_buffer = test_data,
        .rx_buffer = rx_data,
    };

    // 软件CS控制：先拉低CS使能设备
    w25qxx_spi_cs_select(config);
    esp_err_t ret = spi_device_transmit(config->spi_handle, &t);
    // 软件CS控制：拉高CS禁用设备
    w25qxx_spi_cs_deselect(config);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI transmit failed: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "RX: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
             rx_data[0], rx_data[1], rx_data[2], rx_data[3],
             rx_data[4], rx_data[5], rx_data[6], rx_data[7]);

    bool match = true;
    for (int i = 0; i < 8; i++) {
        if (rx_data[i] != test_data[i]) {
            match = false;
            break;
        }
    }

    if (match) {
        ESP_LOGI(TAG, "Loopback test PASSED! MOSI<->MISO connected.");
    } else {
        ESP_LOGW(TAG, "Loopback test FAILED! MOSI<->MISO NOT connected.");
        ESP_LOGW(TAG, "Possible causes:");
        ESP_LOGW(TAG, "  - MOSI/MISO wires swapped");
        ESP_LOGW(TAG, "  - Bad connection");
        ESP_LOGW(TAG, "  - Wrong GPIO pins configured");
    }

    return match;
}

bool w25qxx_spi_write_then_read(w25qxx_config_t *config, const uint8_t *tx_data, uint32_t tx_len, uint8_t *rx_data, uint32_t rx_len) {
    uint32_t total_len = tx_len + rx_len;
    if (total_len == 0 || config == NULL || config->spi_handle == NULL) {
        return total_len == 0;
    }

    /* Always provide both TX and RX buffers: in full-duplex DMA mode the
     * clock only stays alive while the RX DMA is armed. With rx_buffer = NULL
     * the driver disables RX DMA and truncates SCLK, so Page Program / Sector
     * Erase commands never reach the flash chip even though ESP_OK is returned. */
    uint8_t *tx_buf = calloc(1, total_len);
    uint8_t *rx_buf = calloc(1, total_len);
    if (tx_buf == NULL || rx_buf == NULL) {
        free(tx_buf);
        free(rx_buf);
        ESP_LOGE(TAG, "Failed to allocate memory for SPI transaction");
        return false;
    }
    if (tx_len > 0 && tx_data != NULL) {
        memcpy(tx_buf, tx_data, tx_len);
    }

    spi_transaction_t t = {
        .length = total_len * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };

    w25qxx_spi_cs_select(config);
    esp_rom_delay_us(1);
    esp_err_t ret = spi_device_transmit(config->spi_handle, &t);
    esp_rom_delay_us(1);
    w25qxx_spi_cs_deselect(config);

    if (ret == ESP_OK && rx_len > 0 && rx_data != NULL) {
        memcpy(rx_data, rx_buf + tx_len, rx_len);
    }
    free(tx_buf);
    free(rx_buf);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI transaction failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}
