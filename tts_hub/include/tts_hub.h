#ifndef TTS_HUB_H
#define TTS_HUB_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "tts_phrases.h"

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

/* 播放沉槽：tts_hub 把已重采样好的 PCM 交给它（通常由 main 注册，接到 I2S 功放） */
typedef void (*tts_pcm_sink_t)(const int16_t *pcm, size_t bytes);

/* 驱动接口表 —— 每个厂商实现一套填进来 */
typedef struct {
    const char *name;
    esp_err_t (*init)(void);
    esp_err_t (*speak)(const char *text, tts_pcm_cb_t cb, void *ctx);
    bool (*is_busy)(void);
} tts_driver_t;

/**
 * @brief 初始化 TTS 中心
 * @param sink          PCM 播放沉槽（必填，接功放）；可稍后 set 覆盖
 * @param output_rate_hz 目标播放采样率（与功放一致，如 22050）
 */
esp_err_t tts_hub_init(tts_pcm_sink_t sink, uint32_t output_rate_hz);

/* 运行期调整沉槽 / 输出采样率 */
void tts_hub_set_pcm_sink(tts_pcm_sink_t sink);
void tts_hub_set_output_rate(uint32_t hz);

/* ============ 任意文本（走云 TTS，需联网） ============ */
esp_err_t tts_hub_speak(const char *text);
esp_err_t tts_hub_speak_async(const char *text);

/* ============ 固定话术（离线优先，缺失自动回退云） ============ */
esp_err_t tts_hub_say(tts_phrase_t ph);
esp_err_t tts_hub_say_async(tts_phrase_t ph);

/* 是否在线（由上层在 GOT_IP / DISCONNECTED 时更新），影响固定话术兜底 */
void tts_hub_set_online(bool online);
bool tts_hub_online(void);

bool      tts_hub_is_busy(void);
const char *tts_hub_provider_name(void);

#ifdef __cplusplus
}
#endif

#endif
