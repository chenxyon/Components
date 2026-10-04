#ifndef W25QXX_CLI_H
#define W25QXX_CLI_H

#include "w25qxx.h"

#ifdef __cplusplus
extern "C" {
#endif

// CLI命令处理函数
void w25qxx_cli_task(void *arg);
void w25qxx_cli_set_config(w25qxx_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // W25QXX_CLI_H
