#ifndef CONSTANTS_H
#define CONSTANTS_H

#include "driver/gpio.h"
#include "driver/ledc.h"
#include <stdint.h>

/* ==================== 弦数量 ==================== */
#define STRING_COUNT 21

/* ==================== OLED引脚 ==================== */
#define OLED_SDA GPIO_NUM_6
#define OLED_SCL GPIO_NUM_5
#define OLED_ADDR 0x3C

/* ==================== TM1637引脚 ==================== */
#define TM1637_CLK GPIO_NUM_1
#define TM1637_DIO GPIO_NUM_2

/* ==================== 蜂鸣器引脚 ==================== */
#define BUZZER_GPIO GPIO_NUM_40

/* ==================== 74HC595引脚 ==================== */
#define HC595_DS GPIO_NUM_15
#define HC595_SHCP GPIO_NUM_16
#define HC595_STCP GPIO_NUM_17
#define KEY_INPUT GPIO_NUM_14

#define HC595_NUM_CHIPS 1
#define HC595_NUM_KEYS 6

/* ==================== 按键索引 ==================== */
#define KEY_IDX_UP_100 0
#define KEY_IDX_UP_10 1
#define KEY_IDX_UP_1 2
#define KEY_IDX_SWITCH 3
#define KEY_IDX_PLAY 4
#define KEY_IDX_CONFIRM 5

#define KEY_RESET_PIN GPIO_NUM_10

/* ==================== LED引脚 ==================== */
#define LED_STRING GPIO_NUM_38
#define LED_VOLUME GPIO_NUM_39
#define LED_AUTO_ADD GPIO_NUM_41

/* ==================== 蜂鸣器LEDC配置 ==================== */
#define BUZZER_LEDC_TIMER    LEDC_TIMER_0
#define BUZZER_LEDC_CHANNEL  LEDC_CHANNEL_0
#define BUZZER_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_DUTY_RES LEDC_TIMER_10_BIT
#define BUZZER_LEDC_DUTY_MAX 1024

/* ==================== 滚动速度 ==================== */
#define SCROLL_SPEED_MIN 100
#define SCROLL_SPEED_MAX 1000
#define SCROLL_SPEED_STEP 100

/* ==================== 音量 ==================== */
#define VOLUME_MIN 1000
#define VOLUME_MAX 32767
#define VOLUME_STEP 2000

/* ==================== 自动递增 ==================== */
#define AUTO_ADD_DELAY_MS 5000
#define AUTO_ADD_INCREMENT 100

/* ==================== 模式枚举 ==================== */
typedef enum {
    MODE_NONE = 0,
    MODE_STRING = 1,
    MODE_VOLUME = 2,
    MODE_SCROLL = MODE_STRING | MODE_VOLUME,
    MODE_AUTO_ADD = 4
} tuner_mode_t;

/* ==================== 按键事件枚举 ==================== */
typedef enum {
    KEY_NONE = 0,
    KEY_UP_100,
    KEY_UP_10,
    KEY_UP_1,
    KEY_SWITCH,
    KEY_PLAY,
    KEY_CONFIRM,
    KEY_RESET,
    KEY_UP_100_LONG,
    KEY_UP_10_LONG,
    KEY_UP_1_LONG,
    KEY_SWITCH_LONG,
    KEY_PLAY_LONG,
    KEY_CONFIRM_LONG,
    KEY_PLAY_CONFIRM_LONG
} key_event_t;

/* ==================== 默认频率数据 ==================== */
extern const int guzheng_default_freqs[STRING_COUNT];

#endif
