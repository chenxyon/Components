#ifndef W25QXX_H
#define W25QXX_H
#include <inttypes.h>
#include "w25qxx_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

bool w25qxx_init(w25qxx_config_t *config);
void w25qxx_deinit(w25qxx_config_t *config);
bool w25qxx_read_info(w25qxx_config_t *config, w25qxx_info_t *info);
bool w25qxx_read(w25qxx_config_t *config, uint32_t addr, uint8_t *data, uint32_t len);
bool w25qxx_write(w25qxx_config_t *config, uint32_t addr, const uint8_t *data, uint32_t len);
bool w25qxx_erase_sector(w25qxx_config_t *config, uint32_t addr);
bool w25qxx_erase_block(w25qxx_config_t *config, uint32_t addr);
bool w25qxx_erase_chip(w25qxx_config_t *config);
bool w25qxx_is_busy(w25qxx_config_t *config);
void w25qxx_wait_busy(w25qxx_config_t *config);

#ifdef __cplusplus
}
#endif

#endif
