#ifndef HC595_DRIVER_H
#define HC595_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 74HC595 引脚配置
 */
typedef struct {
    int ds_pin;       /* 数据引脚 (DATA) */
    int shcp_pin;     /* 移位时钟引脚 (SHIFT CLOCK) */
    int stcp_pin;     /* 锁存时钟引脚 (STORE CLOCK) */
    int num_chips;    /* 级联芯片数量 (1-N) */
} hc595_config_t;

/**
 * @brief 初始化 74HC595 驱动
 * 
 * @param config 引脚配置
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t hc595_init(const hc595_config_t *config);

/**
 * @brief 写入数据到 74HC595
 * 
 * @param data 要写入的数据 (低位对应第一个芯片的Q0-Q7)
 */
void hc595_write(uint32_t data);

/**
 * @brief 读取当前输出状态
 * 
 * @return uint32_t 当前锁存输出的数据
 */
uint32_t hc595_get_output(void);

/**
 * @brief 设置指定 Q 输出为高电平，其他为低
 * 
 * @param bit_index Q输出索引 (0-7*num_chips-1)
 */
void hc595_set_single(int bit_index);

/**
 * @brief 清除所有输出 (全部置低)
 */
void hc595_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* HC595_DRIVER_H */
