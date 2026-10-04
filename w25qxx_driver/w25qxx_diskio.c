#include "ff.h"
#include "diskio.h"
#include "w25qxx.h"
#include "esp_log.h"

static const char *TAG = "w25qxx_diskio";
static w25qxx_config_t *s_config = NULL;

DSTATUS w25qxx_disk_initialize(BYTE pdrv) {
    if (pdrv != 0 || s_config == NULL) return STA_NOINIT;
    if (!w25qxx_read_info(s_config, &s_config->info)) return STA_NOINIT;
    ESP_LOGI(TAG, "W25QXX Disk Initialized, Size: %u MB", (unsigned int)(s_config->info.total_size / (1024 * 1024)));
    return 0;
}

DSTATUS w25qxx_disk_status(BYTE pdrv) {
    return (pdrv == 0 && s_config != NULL) ? 0 : STA_NOINIT;
}

DRESULT w25qxx_disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || !s_config || !buff) return RES_PARERR;
    uint32_t addr = sector * 512;
    uint32_t len = count * 512;
    return w25qxx_read(s_config, addr, buff, len) ? RES_OK : RES_ERROR;
}

DRESULT w25qxx_disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || !s_config || !buff) return RES_PARERR;
    uint32_t addr = sector * 512;
    uint32_t len = count * 512;
    return w25qxx_write(s_config, addr, buff, len) ? RES_OK : RES_ERROR;
}

DRESULT w25qxx_disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != 0 || !s_config) return RES_PARERR;
    switch (cmd) {
        case GET_SECTOR_COUNT: *(LBA_t*)buff = s_config->info.total_size / 512; break;
        case GET_SECTOR_SIZE:  *(WORD*)buff = 512; break;
        case GET_BLOCK_SIZE:   *(DWORD*)buff = 1; break;
        case CTRL_SYNC:        w25qxx_wait_busy(s_config); break;
        default: return RES_PARERR;
    }
    return RES_OK;
}

void w25qxx_diskio_set_config(w25qxx_config_t *config) {
    s_config = config;
}
