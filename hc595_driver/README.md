# HC595 Driver Component

## 版本: v1.0.0

## 概述

74HC595 串行移位寄存器驱动组件，支持级联多芯片（1-4片），通过 GPIO 控制数据移位和锁存。

## 特性

- 支持级联 1-4 片 74HC595 芯片
- GPIO 控制（DS、SHCP、STCP）
- API 简洁：初始化、写入、清空、单输出控制
- 线程安全（每次操作独立完成移位和锁存）

## API

```c
// 引脚配置
hc595_config_t config = {
    .ds_pin = 15,       // GPIO15
    .shcp_pin = 16,     // GPIO16
    .stcp_pin = 17,     // GPIO17
    .num_chips = 1,     // 1片74HC595
};

// 初始化
hc595_init(&config);

// 写入数据 (Q0=bit0, Q1=bit1, ...)
hc595_write(0x0F);  // Q0-Q3输出高

// 单个Q输出
hc595_set_single(3);  // 仅Q3输出高

// 清空
hc595_clear();

// 读取当前状态
uint32_t state = hc595_get_output();
```

## 引脚说明

| 信号 | 引脚 | 说明 |
|------|------|------|
| DS   | 用户配置 | 数据输入 (串行数据) |
| SHCP | 用户配置 | 移位时钟 (上升沿移位) |
| STCP | 用户配置 | 锁存时钟 (上升沿锁存) |

## 级联说明

- `num_chips = 1`: 8个输出 (Q0-Q7)
- `num_chips = 2`: 16个输出 (Q0-Q15)
- `num_chips = 3`: 24个输出 (Q0-Q23)
- `num_chips = 4`: 32个输出 (Q0-Q31)

数据写入时，低位对应第一个芯片，高位对应后续芯片。

## 依赖

- ESP-IDF >= 5.0.0
- driver (GPIO)
- esp_timer (ets_delay_us)
