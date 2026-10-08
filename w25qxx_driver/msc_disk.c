#include "msc_disk.h"
#include "w25qxx_diskio.h"
#include "w25qxx.h"
#include "esp_log.h"

static const char *TAG = "msc_disk";
static w25qxx_config_t *s_w25qxx_cfg = NULL;

// SCSI 命令处理
int32_t msc_read_sectors(uint32_t lba, uint32_t sector_count, uint8_t *buf) {
    if (!s_w25qxx_cfg) return -1;
    
    ESP_LOGD(TAG, "MSC Read: LBA=%lu, count=%lu", lba, sector_count);
    DRESULT ret = w25qxx_disk_read(0, buf, lba, sector_count);
    return (ret == RES_OK) ? 0 : -1;
}

int32_t msc_write_sectors(uint32_t lba, uint32_t sector_count, uint8_t *buf) {
    if (!s_w25qxx_cfg) return -1;
    
    ESP_LOGD(TAG, "MSC Write: LBA=%lu, count=%lu", lba, sector_count);
    DRESULT ret = w25qxx_disk_write(0, buf, lba, sector_count);
    return (ret == RES_OK) ? 0 : -1;
}

uint32_t msc_get_sector_count(void) {
    if (!s_w25qxx_cfg) return 0;
    return s_w25qxx_cfg->info.total_size / 512;  /* 与 diskio 的 GET_SECTOR_SIZE = 512 对齐 */
}

uint32_t msc_get_sector_size(void) {
    return 512;  /* USB MSC 标准扇区 512B，与 diskio 保持一致；原 4096 会导致 LBA 计算差 8 倍 */
}

uint32_t msc_get_block_size(void) {
    return 1;
}

void msc_disk_set_config(w25qxx_config_t *config) {
    s_w25qxx_cfg = config;
}

// TinyUSB MSC回调函数
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    (void)lun;
    (void)offset;  // bufsize总是扇区对齐的
    
    uint32_t sector_count = bufsize / msc_get_sector_size();
    return msc_read_sectors(lba, sector_count, buffer);
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    (void)lun;
    (void)offset;  // bufsize总是扇区对齐的
    
    uint32_t sector_count = bufsize / msc_get_sector_size();
    return msc_write_sectors(lba, sector_count, buffer);
}

void tud_msc_write10_complete_cb(uint8_t lun) {
    (void)lun;
    // 写入完成，可以在这里添加缓存刷新等操作
    ESP_LOGD(TAG, "MSC Write complete");
}

int32_t tud_msc_get_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size) {
    (void)lun;
    *block_count = msc_get_sector_count();
    *block_size = msc_get_sector_size();
    ESP_LOGI(TAG, "MSC Capacity: %lu blocks, %u bytes/block", *block_count, *block_size);
    return 0;
}

bool tud_msc_is_writable_cb(uint8_t lun) {
    (void)lun;
    return true;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const *cbw, uint8_t *csw) {
    (void)lun;
    (void)cbw;
    
    csw[0] = 0;  // 状态：成功
    csw[2] = 0;  // 残差长度高字节
    csw[3] = 0;  // 残差长度低字节
    
    return 0;
}
