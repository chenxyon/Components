#ifndef W25QXX_SPI_H
#define W25QXX_SPI_H

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define W25QXX_PAGE_SIZE      256
#define W25QXX_SECTOR_SIZE    4096
#define W25QXX_BLOCK_SIZE     65536

#define W25QXX_MANUFACTURER_ID 0xEF

typedef enum {
    W25Q16  = 0x15,
    W25Q32  = 0x16,
    W25Q64  = 0x17,
    W25Q128 = 0x18,
    W25Q256 = 0x19,
    W25Q512 = 0x20,
    W25Q1024 = 0x21,
} w25qxx_model_t;

typedef struct {
    uint32_t total_size;
    uint32_t sector_count;
    uint32_t block_count;
    w25qxx_model_t model;
    uint8_t manufacturer_id;
    bool addr_4byte;
} w25qxx_info_t;

// W25QXX功能标志位
typedef enum {
    W25QXX_FEATURE_NONE     = 0x00,  // 无额外功能
    W25QXX_FEATURE_CLI      = 0x01,  // 启用串口命令交互功能
    W25QXX_FEATURE_FATFS    = 0x02,  // 启用FatFs文件系统功能
    W25QXX_FEATURE_MSC      = 0x04,  // 启用USB MSC功能
} w25qxx_feature_flags_t;

typedef struct {
    int spi_host;
    int cs_pin;
    int sclk_pin;
    int mosi_pin;
    int miso_pin;
    int spi_mode;           // SPI模式: 0或3，默认0
    uint8_t feature_flags;  // 功能标志位，见w25qxx_feature_flags_t
    spi_device_handle_t spi_handle;
    w25qxx_info_t info;
} w25qxx_config_t;

bool w25qxx_spi_init(w25qxx_config_t *config);
void w25qxx_spi_deinit(w25qxx_config_t *config);
bool w25qxx_spi_write_then_read(w25qxx_config_t *config, const uint8_t *tx_data, uint32_t tx_len, uint8_t *rx_data, uint32_t rx_len);
bool w25qxx_spi_loopback_test(w25qxx_config_t *config);
static inline void w25qxx_spi_cs_select(w25qxx_config_t *config) {
    gpio_set_level(config->cs_pin, 0);
}

static inline void w25qxx_spi_cs_deselect(w25qxx_config_t *config) {
    gpio_set_level(config->cs_pin, 1);
}

#ifdef __cplusplus
}
#endif

#endif
