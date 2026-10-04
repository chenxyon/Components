#ifndef DISKIO_W25QXX_H
#define DISKIO_W25QXX_H

#include "ff.h"
#include "diskio.h"
#include "w25qxx.h"

#ifdef __cplusplus
extern "C" {
#endif

DSTATUS w25qxx_disk_initialize(BYTE pdrv);
DSTATUS w25qxx_disk_status(BYTE pdrv);
DRESULT w25qxx_disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count);
DRESULT w25qxx_disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count);
DRESULT w25qxx_disk_ioctl(BYTE pdrv, BYTE cmd, void *buff);

void w25qxx_diskio_set_config(w25qxx_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // DISKIO_W25QXX_H
