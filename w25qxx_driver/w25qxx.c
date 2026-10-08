#include "w25qxx.h"
#include "w25qxx_diskio.h"
#include "msc_disk.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

static const char *TAG = "w25qxx";

#define W25QXX_CMD_WRITE_ENABLE        0x06
#define W25QXX_CMD_WRITE_DISABLE       0x04
#define W25QXX_CMD_READ_STATUS_REG     0x05
#define W25QXX_CMD_WRITE_STATUS_REG    0x01
#define W25QXX_CMD_READ_DATA           0x03
#define W25QXX_CMD_FAST_READ_DATA      0x0B
#define W25QXX_CMD_PAGE_PROGRAM        0x02
#define W25QXX_CMD_SECTOR_ERASE        0x20
#define W25QXX_CMD_BLOCK_ERASE_32K     0x52
#define W25QXX_CMD_BLOCK_ERASE_64K     0xD8
#define W25QXX_CMD_CHIP_ERASE          0xC7
#define W25QXX_CMD_POWER_DOWN          0xB9
#define W25QXX_CMD_RELEASE_POWER_DOWN  0xAB
#define W25QXX_CMD_DEVICE_ID           0x90
#define W25QXX_CMD_JEDEC_ID            0x9F
#define W25QXX_CMD_READ_UNIQUE_ID      0x4B
#define W25QXX_CMD_ENTER_4BYTE_MODE    0xB7
#define W25QXX_CMD_EXIT_4BYTE_MODE     0xE9
#define W25QXX_CMD_READ_DATA_4BYTE     0x13
#define W25QXX_CMD_PAGE_PROGRAM_4BYTE  0x12
#define W25QXX_CMD_SECTOR_ERASE_4BYTE  0x21
#define W25QXX_CMD_BLOCK_ERASE_64K_4BYTE 0xDC

#define W25QXX_PAGE_PROGRAM_TIMEOUT_MS  100  /* 页编程最坏 3ms；FreeRTOS tick 误差+WEL 复查，留 100ms 余量 */
#define W25QXX_SECTOR_ERASE_TIMEOUT_MS  3000
#define W25QXX_BLOCK_ERASE_TIMEOUT_MS   10000
#define W25QXX_CHIP_ERASE_TIMEOUT_MS    180000

static bool w25qxx_write_command(w25qxx_config_t *config, uint8_t cmd) {
    return w25qxx_spi_write_then_read(config, &cmd, 1, NULL, 0);
}

static bool w25qxx_write_command_addr(w25qxx_config_t *config, uint8_t cmd, uint32_t addr) {
    uint8_t tx_data[5] = {cmd};
    if (config->info.addr_4byte) {
        tx_data[1] = (addr >> 24) & 0xFF;
        tx_data[2] = (addr >> 16) & 0xFF;
        tx_data[3] = (addr >> 8) & 0xFF;
        tx_data[4] = addr & 0xFF;
        return w25qxx_spi_write_then_read(config, tx_data, 5, NULL, 0);
    }

    tx_data[1] = (addr >> 16) & 0xFF;
    tx_data[2] = (addr >> 8) & 0xFF;
    tx_data[3] = addr & 0xFF;
    return w25qxx_spi_write_then_read(config, tx_data, 4, NULL, 0);
}

static bool w25qxx_write_command_addr_data(w25qxx_config_t *config, uint8_t cmd, uint32_t addr, const uint8_t *data, uint32_t len) {
    uint32_t addr_len = config->info.addr_4byte ? 4 : 3;
    uint32_t tx_len = 1 + addr_len + len;
    uint8_t *tx_data = malloc(tx_len);
    if (tx_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for SPI write command");
        return false;
    }

    tx_data[0] = cmd;
    if (config->info.addr_4byte) {
        tx_data[1] = (addr >> 24) & 0xFF;
        tx_data[2] = (addr >> 16) & 0xFF;
        tx_data[3] = (addr >> 8) & 0xFF;
        tx_data[4] = addr & 0xFF;
    } else {
        tx_data[1] = (addr >> 16) & 0xFF;
        tx_data[2] = (addr >> 8) & 0xFF;
        tx_data[3] = addr & 0xFF;
    }
    memcpy(tx_data + 1 + addr_len, data, len);

    /* DEBUG: 逐字节打印实际长度内的数据，避免访问 tx_data 越界 */
    {
        char hex[8 * 3 + 1] = {0};
        uint32_t show = tx_len < 8 ? tx_len : 8;
        for (uint32_t i = 0; i < show; i++) {
            snprintf(hex + i * 3, sizeof(hex) - i * 3, "%02X%s",
                     tx_data[i], (i + 1 < show) ? " " : "");
        }
        ESP_LOGI(TAG, "[DEBUG] PageProgram TX (%lu bytes, 前 %u): %s",
                 (unsigned long)tx_len, (unsigned)show, hex);
    }

    bool ok = w25qxx_spi_write_then_read(config, tx_data, tx_len, NULL, 0);
    free(tx_data);
    return ok;
}

static void w25qxx_clear_write_protection(w25qxx_config_t *config);

bool w25qxx_init(w25qxx_config_t *config) {
    if (!w25qxx_spi_init(config)) {
        return false;
    }

    if (!w25qxx_read_info(config, &config->info)) {
        ESP_LOGE(TAG, "Failed to read W25QXX info");
        w25qxx_deinit(config);
        return false;
    }
    
    w25qxx_clear_write_protection(config);

    if (config->info.model >= W25Q512) {
        ESP_LOGI(TAG, "Enabling 4-byte address mode");
        if (!w25qxx_write_command(config, W25QXX_CMD_ENTER_4BYTE_MODE)) {
            ESP_LOGE(TAG, "Failed to enter 4-byte address mode");
            w25qxx_deinit(config);
            return false;
        }
        config->info.addr_4byte = true;
    }
    
    // 根据功能标志位初始化相关功能
    if (config->feature_flags & W25QXX_FEATURE_FATFS) {
        ESP_LOGI(TAG, "Initializing FatFs disk I/O");
        w25qxx_diskio_set_config(config);
    }
    
    if (config->feature_flags & W25QXX_FEATURE_MSC) {
        ESP_LOGI(TAG, "Initializing USB MSC disk");
        msc_disk_set_config(config);
    }
    
    // CLI功能已移至独立组件 w25qxx_cli，由用户在main.c中手动初始化
    if (config->feature_flags & W25QXX_FEATURE_CLI) {
        ESP_LOGI(TAG, "CLI feature enabled (initialize in main.c)");
    }
    
    return true;
}

void w25qxx_deinit(w25qxx_config_t *config) {
    w25qxx_spi_deinit(config);
}

bool w25qxx_read_info(w25qxx_config_t *config, w25qxx_info_t *info) {
    uint8_t tx_data[8] = {W25QXX_CMD_JEDEC_ID, 0, 0, 0, 0, 0, 0, 0};
    uint8_t rx_data[8] = {0};

    spi_transaction_t t = {
        .length = 8 * 8,  // 8字节
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };

    esp_err_t ret = ESP_FAIL;
    int retry_count = 0;
    const int max_retries = 5;

    do {
        memset(rx_data, 0, sizeof(rx_data));
        w25qxx_spi_cs_deselect(config);
        vTaskDelay(pdMS_TO_TICKS(10));

        w25qxx_spi_cs_select(config);
        ret = spi_device_transmit(config->spi_handle, &t);
        w25qxx_spi_cs_deselect(config);

        retry_count++;
        if (ret == ESP_OK && rx_data[1] != 0x00 && rx_data[1] != 0xFF) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    } while (retry_count < max_retries);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read JEDEC ID failed: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "SPI transmit result: %s, retries=%d", esp_err_to_name(ret), retry_count);

    ESP_LOGI(TAG, "JEDEC ID raw: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", 
             rx_data[0], rx_data[1], rx_data[2], rx_data[3], rx_data[4], rx_data[5], rx_data[6], rx_data[7]);

    // SPI全双工：发送命令时W25QXX还未响应，所以数据从第2个字节开始
    // JEDEC ID格式：制造商ID(1字节) + 设备类型(1字节) + 容量代码(1字节)
    // 发送0x9F时W25QXX还未响应(dummy)，发送第2字节时返回制造商ID
    info->manufacturer_id = rx_data[1];  // 制造商ID (0xEF)
    info->model = (w25qxx_model_t)rx_data[3];  // 容量代码 (0x18=W25Q128)

    switch (info->model) {
        case W25Q16:
            info->total_size = 2 * 1024 * 1024;
            break;
        case W25Q32:
            info->total_size = 4 * 1024 * 1024;
            break;
        case W25Q64:
            info->total_size = 8 * 1024 * 1024;
            break;
        case W25Q128:
            info->total_size = 16 * 1024 * 1024;
            break;
        case W25Q256:
            info->total_size = 32 * 1024 * 1024;
            break;
        case W25Q512:
            info->total_size = 64 * 1024 * 1024;
            break;
        case W25Q1024:
            info->total_size = 128 * 1024 * 1024;
            break;
        default:
            ESP_LOGE(TAG, "Unknown flash model: 0x%02X", info->model);
            return false;
    }

    info->sector_count = info->total_size / W25QXX_SECTOR_SIZE;
    info->block_count = info->total_size / W25QXX_BLOCK_SIZE;
    info->addr_4byte = (info->model >= W25Q512);

    ESP_LOGI(TAG, "W25QXX Info: Manufacturer=0x%02X, Model=0x%02X, Size=%" PRIu32 " MB, Sectors=%" PRIu32 ", 4-byte-addr=%s",
             info->manufacturer_id, info->model, info->total_size / (1024 * 1024), info->sector_count,
             info->addr_4byte ? "yes" : "no");

    return info->manufacturer_id == W25QXX_MANUFACTURER_ID;
}

static bool w25qxx_read_status_reg(w25qxx_config_t *config, uint8_t *status) {
    const uint8_t cmd = W25QXX_CMD_READ_STATUS_REG;
    return w25qxx_spi_write_then_read(config, &cmd, 1, status, 1);
}

bool w25qxx_is_busy(w25qxx_config_t *config) {
    uint8_t status;
    return w25qxx_read_status_reg(config, &status) && (status & 0x01) != 0;
}

static bool w25qxx_wait_busy_timeout(w25qxx_config_t *config, uint32_t timeout_ms) {
    TickType_t start = xTaskGetTickCount();
    while (w25qxx_is_busy(config)) {
        if ((xTaskGetTickCount() - start) >= pdMS_TO_TICKS(timeout_ms)) {
            ESP_LOGE(TAG, "Flash operation timed out after %lu ms", (unsigned long)timeout_ms);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return true;
}

void w25qxx_wait_busy(w25qxx_config_t *config) {
    w25qxx_wait_busy_timeout(config, W25QXX_CHIP_ERASE_TIMEOUT_MS);
}

static bool w25qxx_write_enable(w25qxx_config_t *config) {
    return w25qxx_write_command(config, W25QXX_CMD_WRITE_ENABLE);
}

static bool w25qxx_write_status_reg(w25qxx_config_t *config, uint8_t val) {
    if (!w25qxx_write_enable(config)) {
        return false;
    }

    uint8_t tx_data[2] = {W25QXX_CMD_WRITE_STATUS_REG, val};
    if (!w25qxx_spi_write_then_read(config, tx_data, sizeof(tx_data), NULL, 0)) {
        return false;
    }
    return w25qxx_wait_busy_timeout(config, W25QXX_PAGE_PROGRAM_TIMEOUT_MS);
}

static void w25qxx_clear_write_protection(w25qxx_config_t *config) {
    uint8_t sr;
    if (!w25qxx_read_status_reg(config, &sr)) {
        ESP_LOGE(TAG, "无法读取状态寄存器，跳过写保护检查");
        return;
    }

    ESP_LOGI(TAG, "状态寄存器: 0x%02X (SRP=%d TB=%d BP2=%d BP1=%d BP0=%d WEL=%d WIP=%d)",
             sr, (sr >> 6) & 1, (sr >> 5) & 1, (sr >> 4) & 1,
             (sr >> 3) & 1, (sr >> 2) & 1, (sr >> 1) & 1, sr & 1);

    if (sr & 0x7C) {
        ESP_LOGW(TAG, "写保护已启用，正在清除...");
        if (!w25qxx_write_status_reg(config, 0x00) || !w25qxx_read_status_reg(config, &sr)) {
            ESP_LOGE(TAG, "写保护清除失败：无法更新或读取状态寄存器");
            return;
        }
        ESP_LOGI(TAG, "清除后状态寄存器: 0x%02X", sr);
        if (sr & 0x7C) {
            ESP_LOGE(TAG, "写保护清除失败！SRP位可能锁定了状态寄存器");
        } else {
            ESP_LOGI(TAG, "写保护已成功清除");
        }
    } else {
        ESP_LOGI(TAG, "无写保护，可正常写入");
    }
}

bool w25qxx_read(w25qxx_config_t *config, uint32_t addr, uint8_t *data, uint32_t len) {
    if (config == NULL || data == NULL || len == 0 || addr > config->info.total_size || len > config->info.total_size - addr) {
        return false;
    }

    uint8_t cmd = config->info.addr_4byte ? W25QXX_CMD_READ_DATA_4BYTE : W25QXX_CMD_READ_DATA;
    uint32_t addr_len = config->info.addr_4byte ? 4 : 3;
    uint32_t tx_len = 1 + addr_len;
    uint8_t tx_data[5] = {cmd};
    if (config->info.addr_4byte) {
        tx_data[1] = (addr >> 24) & 0xFF;
        tx_data[2] = (addr >> 16) & 0xFF;
        tx_data[3] = (addr >> 8) & 0xFF;
        tx_data[4] = addr & 0xFF;
    } else {
        tx_data[1] = (addr >> 16) & 0xFF;
        tx_data[2] = (addr >> 8) & 0xFF;
        tx_data[3] = addr & 0xFF;
    }

    return w25qxx_spi_write_then_read(config, tx_data, tx_len, data, len);
}

bool w25qxx_write(w25qxx_config_t *config, uint32_t addr, const uint8_t *data, uint32_t len) {
    if (config == NULL || data == NULL || len == 0 || addr > config->info.total_size || len > config->info.total_size - addr) {
        return false;
    }

    uint32_t bytes_written = 0;
    uint8_t cmd = config->info.addr_4byte ? W25QXX_CMD_PAGE_PROGRAM_4BYTE : W25QXX_CMD_PAGE_PROGRAM;

    while (bytes_written < len) {
        uint32_t offset = addr & (W25QXX_PAGE_SIZE - 1);
        uint32_t bytes_to_write = W25QXX_PAGE_SIZE - offset;
        if (bytes_to_write > len - bytes_written) {
            bytes_to_write = len - bytes_written;
        }

        if (!w25qxx_write_enable(config)) {
            ESP_LOGE(TAG, "Write enable command failed");
            return false;
        }

        if (!w25qxx_write_command_addr_data(config, cmd, addr, &data[bytes_written], bytes_to_write)) {
            ESP_LOGE(TAG, "Page program command failed at address 0x%08lX", (unsigned long)addr);
            return false;
        }

        /* 状态诊断日志降级为 ESP_LOGD：生产环境串口输出会严重拖慢页编程 */
        uint8_t sr_after_pp = 0;
        w25qxx_read_status_reg(config, &sr_after_pp);
        ESP_LOGD(TAG, "[DEBUG] SR after PageProgram: 0x%02X (WIP=%d WEL=%d)",
                 sr_after_pp, sr_after_pp & 0x01, (sr_after_pp >> 1) & 1);

        if (!w25qxx_wait_busy_timeout(config, W25QXX_PAGE_PROGRAM_TIMEOUT_MS)) {
            ESP_LOGE(TAG, "Page program failed at address 0x%08lX", (unsigned long)addr);
            return false;
        }

        uint8_t sr_after_wait = 0;
        w25qxx_read_status_reg(config, &sr_after_wait);
        ESP_LOGD(TAG, "[DEBUG] SR after wait_busy: 0x%02X (WIP=%d WEL=%d)",
                 sr_after_wait, sr_after_wait & 0x01, (sr_after_wait >> 1) & 1);

        bytes_written += bytes_to_write;
        addr += bytes_to_write;
    }

    return true;
}

bool w25qxx_erase_sector(w25qxx_config_t *config, uint32_t addr) {
    if (config == NULL || addr >= config->info.total_size) return false;

    /* 校验 4KB 对齐：不对齐的擦除会破坏相邻扇区，属于编程错误 */
    if (addr % W25QXX_SECTOR_SIZE != 0) {
        ESP_LOGE(TAG, "erase_sector: addr 0x%08lX 未按 %u 字节对齐",
                 (unsigned long)addr, W25QXX_SECTOR_SIZE);
        return false;
    }

    /* Wait for any prior operation to complete (matches official STM32 reference) */
    if (!w25qxx_wait_busy_timeout(config, W25QXX_SECTOR_ERASE_TIMEOUT_MS)) {
        ESP_LOGE(TAG, "Previous operation still in progress before sector erase");
        return false;
    }

    uint8_t cmd = config->info.addr_4byte ? W25QXX_CMD_SECTOR_ERASE_4BYTE : W25QXX_CMD_SECTOR_ERASE;
    if (!w25qxx_write_enable(config)) {
        ESP_LOGE(TAG, "Write enable failed before sector erase at 0x%08lX", (unsigned long)addr);
        return false;
    }

    /* Verify WEL bit is set (matches official reference robustness) */
    uint8_t sr;
    if (!w25qxx_read_status_reg(config, &sr) || !(sr & 0x02)) {
        ESP_LOGE(TAG, "WEL not set after WriteEnable for sector erase! SR=0x%02X", sr);
        return false;
    }

    if (!w25qxx_write_command_addr(config, cmd, addr)) {
        ESP_LOGE(TAG, "Sector erase command failed at 0x%08lX", (unsigned long)addr);
        return false;
    }
    return w25qxx_wait_busy_timeout(config, W25QXX_SECTOR_ERASE_TIMEOUT_MS);
}

bool w25qxx_erase_block(w25qxx_config_t *config, uint32_t addr) {
    if (config == NULL || addr >= config->info.total_size) return false;

    /* 校验 64KB 对齐 */
    if (addr % W25QXX_BLOCK_SIZE != 0) {
        ESP_LOGE(TAG, "erase_block: addr 0x%08lX 未按 %u 字节对齐",
                 (unsigned long)addr, W25QXX_BLOCK_SIZE);
        return false;
    }

    /* Wait for any prior operation to complete */
    if (!w25qxx_wait_busy_timeout(config, W25QXX_BLOCK_ERASE_TIMEOUT_MS)) {
        ESP_LOGE(TAG, "Previous operation still in progress before block erase");
        return false;
    }

    uint8_t cmd = config->info.addr_4byte ? W25QXX_CMD_BLOCK_ERASE_64K_4BYTE : W25QXX_CMD_BLOCK_ERASE_64K;
    if (!w25qxx_write_enable(config)) {
        ESP_LOGE(TAG, "Write enable failed before block erase at 0x%08lX", (unsigned long)addr);
        return false;
    }

    /* Verify WEL bit is set */
    uint8_t sr;
    if (!w25qxx_read_status_reg(config, &sr) || !(sr & 0x02)) {
        ESP_LOGE(TAG, "WEL not set after WriteEnable for block erase! SR=0x%02X", sr);
        return false;
    }

    if (!w25qxx_write_command_addr(config, cmd, addr)) {
        ESP_LOGE(TAG, "Block erase command failed at 0x%08lX", (unsigned long)addr);
        return false;
    }
    return w25qxx_wait_busy_timeout(config, W25QXX_BLOCK_ERASE_TIMEOUT_MS);
}

bool w25qxx_erase_chip(w25qxx_config_t *config) {
    if (config == NULL) {
        return false;
    }

    /* Wait for any prior operation to complete */
    if (!w25qxx_wait_busy_timeout(config, W25QXX_CHIP_ERASE_TIMEOUT_MS)) {
        ESP_LOGE(TAG, "Previous operation still in progress before chip erase");
        return false;
    }

    if (!w25qxx_write_enable(config)) {
        ESP_LOGE(TAG, "Write enable failed before chip erase");
        return false;
    }

    /* Verify WEL bit is set */
    uint8_t sr;
    if (!w25qxx_read_status_reg(config, &sr) || !(sr & 0x02)) {
        ESP_LOGE(TAG, "WEL not set after WriteEnable for chip erase! SR=0x%02X", sr);
        return false;
    }

    if (!w25qxx_write_command(config, W25QXX_CMD_CHIP_ERASE)) {
        ESP_LOGE(TAG, "Chip erase command failed");
        return false;
    }
    return w25qxx_wait_busy_timeout(config, W25QXX_CHIP_ERASE_TIMEOUT_MS);
}
