/* tts_hub —— 厂商无关的统一入口（云 TTS 为主 + 离线片段兜底）
 *
 * 策略：
 *   - 固定话术 tts_hub_say()：有离线片段→直接播（离线、低延迟）；
 *     无片段→在线则走云 TTS，离线则仅日志提示。
 *   - 任意文本 tts_hub_speak()：走云 TTS（需联网）。
 *
 * 播放解耦：本组件不直接依赖 I2S/功放，改为把重采样后的 PCM
 * 交给上层注册的 sink（main 里接 max98357_play）。这样组件可复用、
 * 不产生 "组件依赖 main" 的反向依赖。
 */
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "tts_hub.h"
#include "tts_config.h"
#include "tts_xfyun.h"

static const char *TAG = "TTS_HUB";

static const tts_driver_t *s_drvs[] = {
    [TTS_PROVIDER_XFYUN] = &tts_drv_xfyun,
    [TTS_PROVIDER_BAIDU] = NULL,
    [TTS_PROVIDER_ALI]   = NULL,
};

static volatile bool   s_online   = true;
static tts_pcm_sink_t  s_sink     = NULL;
static uint32_t        s_out_rate = 22050;
/* 云/片段源采样率统一按 16000 处理（讯飞 raw 与生成脚本均输出 16k） */
#define TTS_SRC_HZ 16000

static const tts_driver_t *active_drv(void)
{
    tts_provider_t p = (tts_provider_t)TTS_ACTIVE_PROVIDER;
    if ((int)p < 0 || p >= (int)(sizeof(s_drvs) / sizeof(s_drvs[0])))
        return NULL;
    return s_drvs[p];
}

/* 播放 16k s16le 单声道 PCM：重采样到输出采样率后分片交给 sink */
static void tts_play_pcm16(const int16_t *in, size_t samples)
{
    if (!in || samples == 0) return;
    if (!s_sink) { ESP_LOGW(TAG, "未注册 PCM sink，丢弃音频"); return; }

    const uint32_t DST = s_out_rate ? s_out_rate : TTS_SRC_HZ;
    size_t out_n = samples * DST / TTS_SRC_HZ;
    int16_t *out = malloc(out_n * sizeof(int16_t));
    if (!out) {
        s_sink(in, samples * sizeof(int16_t));
        return;
    }
    for (size_t i = 0; i < out_n; i++) {
        float pos = (float)i * TTS_SRC_HZ / DST;
        size_t idx = (size_t)pos;
        float frac = pos - idx;
        int16_t a = in[idx < samples ? idx : samples - 1];
        int16_t b = in[(idx + 1) < samples ? idx + 1 : samples - 1];
        out[i] = (int16_t)(a + (b - a) * frac);
    }
    const size_t CHUNK = 2048;
    for (size_t o = 0; o < out_n; o += CHUNK) {
        size_t n = (out_n - o) > CHUNK ? CHUNK : (out_n - o);
        s_sink(out + o, n * sizeof(int16_t));
    }
    free(out);
}

static void cloud_play_cb(const int16_t *in, size_t samples,
                          uint32_t src_hz, void *ctx)
{
    (void)src_hz; (void)ctx;
    tts_play_pcm16(in, samples);
}

esp_err_t tts_hub_init(tts_pcm_sink_t sink, uint32_t output_rate_hz)
{
    if (sink)      s_sink = sink;
    if (output_rate_hz) s_out_rate = output_rate_hz;

    const tts_driver_t *d = active_drv();
    if (!d) {
        ESP_LOGE(TAG, "厂商未实现 (id=%d)", (int)TTS_ACTIVE_PROVIDER);
        return ESP_ERR_NOT_SUPPORTED;
    }
    ESP_LOGI(TAG, "当前厂商: %s  输出采样率=%lu", d->name,
             (unsigned long)s_out_rate);
    return d->init ? d->init() : ESP_OK;
}

void tts_hub_set_pcm_sink(tts_pcm_sink_t sink) { s_sink = sink; }
void tts_hub_set_output_rate(uint32_t hz)      { if (hz) s_out_rate = hz; }

const char *tts_hub_provider_name(void)
{
    const tts_driver_t *d = active_drv();
    return d ? d->name : "none";
}

bool tts_hub_is_busy(void)
{
    const tts_driver_t *d = active_drv();
    return d && d->is_busy ? d->is_busy() : false;
}

void tts_hub_set_online(bool online) { s_online = online; }
bool tts_hub_online(void)            { return s_online; }

static bool offline_have(tts_phrase_t ph)
{
    return ph < TTS_PHRASE_MAX && tts_clips[ph].data && tts_clips[ph].len >= 2;
}
static void offline_play(tts_phrase_t ph)
{
    tts_play_pcm16((const int16_t *)tts_clips[ph].data, tts_clips[ph].len / 2);
}

esp_err_t tts_hub_speak(const char *text)
{
    const tts_driver_t *d = active_drv();
    if (!d || !d->speak) return ESP_ERR_NOT_SUPPORTED;
    if (!s_online) {
        ESP_LOGW(TAG, "离线，无法合成任意文本: %s", text);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "[%s] 合成: %s", d->name, text);
    esp_err_t r = d->speak(text, cloud_play_cb, NULL);
    ESP_LOGI(TAG, "[%s] 播放%s", d->name, r == ESP_OK ? "完成" : "失败");
    return r;
}

esp_err_t tts_hub_say(tts_phrase_t ph)
{
    if (ph >= TTS_PHRASE_MAX) return ESP_ERR_INVALID_ARG;

    if (offline_have(ph)) {
        ESP_LOGI(TAG, "离线片段播放: %s", tts_phrase_text(ph));
        offline_play(ph);
        return ESP_OK;
    }
    const char *text = tts_phrase_text(ph);
    if (s_online) {
        ESP_LOGI(TAG, "无离线片段，回退云 TTS: %s", text);
        return tts_hub_speak(text);
    }
    ESP_LOGW(TAG, "离线且无片段，跳过: %s", text);
    return ESP_ERR_NOT_FOUND;
}

/* ==================== 异步封装 ==================== */

typedef struct { char *text; tts_phrase_t ph; bool is_phrase; } async_arg_t;

static void async_task(void *arg)
{
    async_arg_t *a = arg;
    if (a->is_phrase) tts_hub_say(a->ph);
    else              tts_hub_speak(a->text);
    free(a->text);
    free(a);
    vTaskDelete(NULL);
}

static esp_err_t spawn(async_arg_t *a)
{
    if (xTaskCreate(async_task, "tts_hub", 8192, a, 6, NULL) != pdPASS) {
        free(a->text); free(a);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t tts_hub_speak_async(const char *text)
{
    async_arg_t *a = malloc(sizeof(*a));
    if (!a) return ESP_ERR_NO_MEM;
    a->text = strdup(text); a->ph = 0; a->is_phrase = false;
    if (!a->text) { free(a); return ESP_ERR_NO_MEM; }
    return spawn(a);
}

esp_err_t tts_hub_say_async(tts_phrase_t ph)
{
    async_arg_t *a = malloc(sizeof(*a));
    if (!a) return ESP_ERR_NO_MEM;
    a->text = NULL; a->ph = ph; a->is_phrase = true;
    return spawn(a);
}
