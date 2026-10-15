/* tts_hub —— 厂商无关的统一入口
 * 播放侧：收到驱动回调的 PCM（16k）后线性重采样到喇叭采样率再送 MAX98357。
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

/* PCM 输出槽：默认空，板级必须先 tts_hub_set_sink() 注册，否则 speak 直接失败 */
static tts_pcm_sink_cb_t s_sink = NULL;
static void *s_sink_ctx = NULL;

esp_err_t tts_hub_set_sink(tts_pcm_sink_cb_t sink, void *ctx)
{
    if (!sink) {
        return ESP_ERR_INVALID_ARG;
    }
    s_sink = sink;
    s_sink_ctx = ctx;
    return ESP_OK;
}

/* 厂商表：新增厂商时加一行 + 实现对应 tts_drv_xxx 即可 */
static const tts_driver_t *s_drvs[] = {
    [TTS_PROVIDER_XFYUN] = &tts_drv_xfyun,
    [TTS_PROVIDER_BAIDU] = NULL,   /* 待实现 */
    [TTS_PROVIDER_ALI]   = NULL,   /* 待实现 */
};

static const tts_driver_t *active_drv(void)
{
    tts_provider_t p = (tts_provider_t)TTS_ACTIVE_PROVIDER;
    if ((int)p < 0 || p >= (int)(sizeof(s_drvs) / sizeof(s_drvs[0])))
        return NULL;
    return s_drvs[p];
}

esp_err_t tts_hub_init(void)
{
    const tts_driver_t *d = active_drv();
    if (!d) {
        ESP_LOGE(TAG, "厂商未实现 (id=%d)", (int)TTS_ACTIVE_PROVIDER);
        return ESP_ERR_NOT_SUPPORTED;
    }
    ESP_LOGI(TAG, "当前厂商: %s", d->name);
    return d->init ? d->init() : ESP_OK;
}

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

/* 播放侧回调：16k -> 目标采样率（默认 22050）线性插值，分片送输出槽 */
static void play_cb(const int16_t *in, size_t samples,
                    uint32_t src_hz, void *ctx)
{
    uint32_t dst_hz = 22050;
    if (ctx) dst_hz = *(uint32_t *)ctx;  /* 允许调用方覆盖目标采样率 */
    (void)src_hz;                        /* 讯飞固定 16000 */

    const uint32_t SRC = 16000;
    size_t out_n = samples * dst_hz / SRC;
    int16_t *out = malloc(out_n * sizeof(int16_t));
    if (!out) {  /* 内存不足则按原采样直接送输出槽（会变调，仅保底） */
        if (s_sink) s_sink(in, samples * 2, s_sink_ctx);
        return;
    }
    for (size_t i = 0; i < out_n; i++) {
        float pos = (float)i * SRC / dst_hz;
        size_t idx = (size_t)pos;
        float frac = pos - idx;
        int16_t a = in[idx < samples ? idx : samples - 1];
        int16_t b = in[(idx + 1) < samples ? idx + 1 : samples - 1];
        out[i] = (int16_t)(a + (b - a) * frac);
    }
    if (!s_sink) {
        ESP_LOGE(TAG, "未注册 PCM 输出槽，先调 tts_hub_set_sink()");
        free(out);
        return;
    }
    const size_t CHUNK = 2048;
    for (size_t o = 0; o < out_n; o += CHUNK) {
        size_t n = (out_n - o) > CHUNK ? CHUNK : (out_n - o);
        s_sink(out + o, n * sizeof(int16_t), s_sink_ctx);
    }
    free(out);
}

esp_err_t tts_hub_speak(const char *text)
{
    const tts_driver_t *d = active_drv();
    if (!d || !d->speak) return ESP_ERR_NOT_SUPPORTED;
    if (!s_sink) {
        ESP_LOGE(TAG, "未注册 PCM 输出槽，先调 tts_hub_set_sink()");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "[%s] 合成: %s", d->name, text);
    esp_err_t r = d->speak(text, play_cb, NULL);
    ESP_LOGI(TAG, "[%s] 播放%s", d->name, r == ESP_OK ? "完成" : "失败");
    return r;
}

typedef struct {
    char *text;
} async_arg_t;

static void speak_task(void *arg)
{
    async_arg_t *a = arg;
    tts_hub_speak(a->text);
    free(a->text);
    free(a);
    vTaskDelete(NULL);
}

esp_err_t tts_hub_speak_async(const char *text)
{
    if (tts_hub_is_busy()) return ESP_ERR_INVALID_STATE;
    async_arg_t *a = malloc(sizeof(*a));
    if (!a) return ESP_ERR_NO_MEM;
    a->text = strdup(text);
    if (!a->text) { free(a); return ESP_ERR_NO_MEM; }
    if (xTaskCreate(speak_task, "tts_hub", 8192, a, 6, NULL) != pdPASS) {
        free(a->text); free(a);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
