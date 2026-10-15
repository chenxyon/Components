#ifndef TTS_HUB_H
#define TTS_HUB_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 厂商枚举 —— 新增厂商时追加即可 */
typedef enum {
    TTS_PROVIDER_XFYUN = 0,   /* 科大讯飞 WebSocket */
    TTS_PROVIDER_BAIDU,       /* 百度 HTTP（待实现） */
    TTS_PROVIDER_ALI,         /* 阿里 HTTP（待实现） */
} tts_provider_t;

/* PCM 回调：厂商无关，解码出的 16-bit 单声道 PCM 都走这里 */
typedef void (*tts_pcm_cb_t)(const int16_t *pcm, size_t samples,
                             uint32_t sample_rate_hz, void *ctx);

/* PCM 输出槽：板级把喇叭推送函数注册进来，tts_hub 只管合成，不管 I2S 细节。
 * 这样 tts_hub 无需依赖具体功放驱动（zhineng 的 max98357 在 main 里自带）。 */
typedef esp_err_t (*tts_pcm_sink_cb_t)(const int16_t *pcm, size_t bytes, void *ctx);

/* 驱动接口表 —— 每个厂商实现一套填进来 */
typedef struct {
    const char *name;
    esp_err_t (*init)(void);
    /* text: UTF-8；cb: 每解出一帧 PCM 回调一次；done: 全部结束（含失败） */
    esp_err_t (*speak)(const char *text, tts_pcm_cb_t cb, void *ctx);
    bool (*is_busy)(void);
} tts_driver_t;

esp_err_t tts_hub_init(void);

/* 注册 PCM 输出槽：板级把喇叭推送函数（如 max98357_play 封装）传进来 */
esp_err_t tts_hub_set_sink(tts_pcm_sink_cb_t sink, void *ctx);

/* 用 tts_config.h 里 TTS_ACTIVE_PROVIDER 指定的厂商播报 */
esp_err_t tts_hub_speak(const char *text);
esp_err_t tts_hub_speak_async(const char *text);
bool      tts_hub_is_busy(void);
const char *tts_hub_provider_name(void);

#ifdef __cplusplus
}
#endif

#endif
