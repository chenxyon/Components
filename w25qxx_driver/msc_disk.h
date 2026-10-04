#ifndef MSC_DISK_H
#define MSC_DISK_H

#include "w25qxx.h"

#ifdef __cplusplus
extern "C" {
#endif

// MSC磁盘操作函数
int32_t msc_read_sectors(uint32_t lba, uint32_t sector_count, uint8_t *buf);
int32_t msc_write_sectors(uint32_t lba, uint32_t sector_count, uint8_t *buf);
uint32_t msc_get_sector_count(void);
uint32_t msc_get_sector_size(void);
uint32_t msc_get_block_size(void);
void msc_disk_set_config(w25qxx_config_t *config);

// TinyUSB MSC回调函数（需要在tinyusb_config.h中声明）
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize);
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize);
void tud_msc_write10_complete_cb(uint8_t lun);
int32_t tud_msc_get_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size);
bool tud_msc_is_writable_cb(uint8_t lun);
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const *cbw, uint8_t *csw);

#ifdef __cplusplus
}
#endif

#endif // MSC_DISK_H
