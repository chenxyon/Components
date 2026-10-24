/* 唤醒引擎：讯飞在线语音听写（IAT）+ 唤醒词文本匹配
 *
 * 协议采用最通用、每个讯飞应用默认开通的「语音听写（流式版）」v2：
 *   wss://iat-api.xfyun.cn/v2/iat
 *   首帧带 common/business/data，之后按 40ms/1280 字节流式发音频，末帧 status=2。
 *   返回 data.result.ws[].cw[].w 为识别文本，data.result.ls=true 为整句结束。
 *
 * 说明：早期误用的 iat.xf-yun.com/v1 端点在新版按 domain 路由，domain=lm
 * 会返回 10404 "no category route found"，故改用 v2 + domain=iat。
 */
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_websocket_client.h"
#include "esp_crt_bundle.h"
#include "mbedtls/base64.h"
#include "cJSON.h"

#include "inmp441.h"
#include "xfyun_auth.h"
#include "kws_config.h"

#include "kws_engine.h"

static const char *TAG = "KWS_XFYUN";

#define XF_HOST  "iat-api.xfyun.cn"
#define XF_PATH  "/v2/iat"

/* 讯飞听写推荐：每 40ms 发 1280 字节（=640 采样@16kHz，正好实时）*/
#define IAT_CHUNK_BYTES   1280
#define IAT_CHUNK_MS      40
#define IAT_RX_BUF        4096   /* v2 返回报文较小，够用 */
#define IAT_TEXT_MAX      512    /* 整句累积缓冲 */

/* 引擎会话状态 */
static volatile bool s_running;
static volatile bool s_session_busy;
static volatile bool s_task_alive;
static volatile bool s_ws_error;
static esp_websocket_client_handle_t s_cli;
static char s_rx[IAT_RX_BUF];
static size_t s_rxlen;
static int s_seq;
static char s_text[IAT_TEXT_MAX];   /* 跨分片累积整句 */

/* ==================== 发送 ==================== */

static const char *k_appid(void)
{
    return (ASR_XFYUN_APPID[0] != '\0') ? ASR_XFYUN_APPID : TTS_XFYUN_APPID;
}
static const char *k_key(void)
{
    return (ASR_XFYUN_API_KEY[0] != '\0') ? ASR_XFYUN_API_KEY : TTS_XFYUN_API_KEY;
}
static const char *k_secret(void)
{
    return (ASR_XFYUN_API_SECRET[0] != '\0') ? ASR_XFYUN_API_SECRET
                                              : TTS_XFYUN_API_SECRET;
}

/* v2 音频格式串：16k 16bit 小端单声道 */
#define IAT_AUDIO_FORMAT "audio/L16;rate=16000"

/* 发一帧音频。status: 0=首帧(带参数) 1=中间 2=结束 */
static void send_audio_frame(const uint8_t *pcm, size_t len, int status)
{
    static char b64[IAT_CHUNK_BYTES / 3 * 4 + 8];
    char *js;
    size_t olen = 0;

    b64[0] = '\0';
    if (len) {
        if (mbedtls_base64_encode((unsigned char *)b64, sizeof(b64), &olen,
                                  pcm, len) != 0) {
            return;
        }
        b64[olen] = '\0';
    }

    if (status == 0) {
        /* 首帧：common + business + data */
        js = malloc(1024 + olen);
        if (!js) return;
        snprintf(js, 1024 + olen,
                 "{\"common\":{\"app_id\":\"%s\"},"
                 "\"business\":{\"language\":\"zh_cn\",\"domain\":\"iat\","
                 "\"accent\":\"mandarin\",\"vad_eos\":%d},"
                 "\"data\":{\"status\":0,\"format\":\"" IAT_AUDIO_FORMAT "\","
                 "\"encoding\":\"raw\",\"audio\":\"%s\"}}",
                 k_appid(), CONFIG_KWS_IAT_EOS_MS, b64);
    } else {
        js = malloc(384 + olen);
        if (!js) return;
        snprintf(js, 384 + olen,
                 "{\"data\":{\"status\":%d,\"format\":\"" IAT_AUDIO_FORMAT "\","
                 "\"encoding\":\"raw\",\"audio\":\"%s\"}}",
                 status, b64);
    }

    esp_websocket_client_send_text(s_cli, js, strlen(js), pdMS_TO_TICKS(200));
    free(js);
    s_seq++;
}

/* ==================== 接收解析 ==================== */

/* 把一段 result.ws[].cw[].w 拼成字符串，追加到 s_text */
static void append_words(cJSON *result)
{
    cJSON *ws = cJSON_GetObjectItem(result, "ws");
    int nseg = cJSON_GetArraySize(ws);
    for (int s = 0; s < nseg; s++) {
        cJSON *seg = cJSON_GetArrayItem(ws, s);
        cJSON *cw = cJSON_GetObjectItem(seg, "cw");
        int ncw = cJSON_GetArraySize(cw);
        for (int i = 0; i < ncw; i++) {
            cJSON *w = cJSON_GetObjectItem(cJSON_GetArrayItem(cw, i), "w");
            if (w && w->valuestring) {
                size_t cur = strlen(s_text);
                size_t l = strlen(w->valuestring);
                if (cur + l < sizeof(s_text)) {
                    memcpy(s_text + cur, w->valuestring, l);
                    s_text[cur + l] = '\0';
                }
            }
        }
    }
}

static void handle_response(const char *json, size_t len)
{
    cJSON *r = cJSON_ParseWithLength(json, len);
    if (!r) return;

    /* v2 协议：code/message 在顶层，识别结果在 data.result，结束帧 data.status==2 */
    cJSON *code = cJSON_GetObjectItem(r, "code");
    int c = code ? code->valueint : 0;
    if (c != 0) {
        cJSON *msg = cJSON_GetObjectItem(r, "message");
        char buf[128];
        snprintf(buf, sizeof(buf), "code=%d msg=%s", c,
                 (msg && msg->valuestring) ? msg->valuestring : "(无)");
        kws_engine_emit_error(buf);
        s_session_busy = false;
        cJSON_Delete(r);
        return;
    }

    cJSON *data = cJSON_GetObjectItem(r, "data");
    if (data) {
        cJSON *res = cJSON_GetObjectItem(data, "result");
        bool final = false;
        if (res) {
            append_words(res);
            cJSON *ls = cJSON_GetObjectItem(res, "ls");
            final = ls && ls->valueint;
        }
        if (final && s_text[0]) {
            ESP_LOGI(TAG, "识别整句: %s", s_text);
            kws_engine_emit(s_text, true);
            s_text[0] = '\0';
        }
        cJSON *st = cJSON_GetObjectItem(data, "status");
        if (st && st->valueint == 2) s_session_busy = false;
    }

    cJSON_Delete(r);
}

static void ws_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_websocket_event_data_t *ev = data;
    (void)arg; (void)base;

    switch (id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "听写已连接，等待音频");
        s_session_busy = true;
        break;

    case WEBSOCKET_EVENT_DATA: {
        if (s_rxlen + ev->data_len >= sizeof(s_rx)) {
            ESP_LOGW(TAG, "听写返回报文超长(%u)，丢弃", (unsigned)ev->data_len);
            s_rxlen = 0;
            break;
        }
        memcpy(s_rx + s_rxlen, ev->data_ptr, ev->data_len);
        s_rxlen += ev->data_len;

        /* 能解析成完整 JSON 才处理，否则等后续分片 */
        if (cJSON_ParseWithLength(s_rx, s_rxlen) != NULL) {
            handle_response(s_rx, s_rxlen);
            s_rxlen = 0;
        }
        break;
    }

    case WEBSOCKET_EVENT_ERROR:
    case WEBSOCKET_EVENT_CLOSED:
    case WEBSOCKET_EVENT_DISCONNECTED:
        s_session_busy = false;
        s_ws_error = true;
        break;

    default:
        break;
    }
}

/* ==================== 会话任务 ==================== */

static void session_task(void *arg)
{
    (void)arg;
    char url[2600];

    if (xfyun_wait_sntp(20) != ESP_OK) {
        kws_engine_emit_error("SNTP 未同步，鉴权会失败");
        goto finish;
    }
    if (xfyun_build_auth_url(XF_HOST, XF_PATH, k_key(), k_secret(),
                             url, sizeof(url)) != ESP_OK) {
        kws_engine_emit_error("鉴权 URL 生成失败");
        goto finish;
    }

    s_seq  = 1;
    s_rxlen = 0;
    s_text[0] = '\0';
    s_ws_error = false;

    ESP_LOGI(TAG, "建连前内存: 总剩=%u 最大连续块=%u | 内部RAM剩=%u 内部最大块=%u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    esp_websocket_client_config_t cfg = {
        .uri = url,
        .buffer_size = 2048,
        .task_stack = 4096,
        .disable_auto_reconnect = true,
        .network_timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    s_cli = esp_websocket_client_init(&cfg);
    if (!s_cli) {
        kws_engine_emit_error("websocket 客户端创建失败");
        goto finish;
    }
    esp_websocket_register_events(s_cli, WEBSOCKET_EVENT_ANY, ws_event, NULL);
    esp_websocket_client_start(s_cli);

    static int16_t pcm[IAT_CHUNK_BYTES / 2];
    bool connected = false;
    bool ended = false;

    for (int i = 0; i < 100 && s_running; i++) {
        if (s_session_busy) { connected = true; break; }
        if (s_ws_error) break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!connected) {
        ESP_LOGW(TAG, "听写连接失败/超时");
        goto finish;
    }

    ESP_LOGI(TAG, "开始流式识别（%u ms/%u 字节）", (unsigned)IAT_CHUNK_MS,
             (unsigned)IAT_CHUNK_BYTES);
    send_audio_frame(NULL, 0, 0);

    int64_t deadline = esp_timer_get_time() + 55LL * 1000 * 1000;
    int guard = 0;

    while (s_running && !ended) {
        if (!s_session_busy) { ended = true; break; }
        if (esp_timer_get_time() > deadline) { ended = true; break; }

        size_t got = 0;
        esp_err_t err = inmp441_read(pcm, sizeof(pcm), &got, pdMS_TO_TICKS(300));
        if (err == ESP_OK && got > 0) {
            send_audio_frame((const uint8_t *)pcm, got, 1);
        } else if (++guard > 20) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(IAT_CHUNK_MS));
    }

    if (s_running) {
        send_audio_frame(NULL, 0, 2);
        for (int i = 0; i < 15 && s_session_busy; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

finish:
    s_session_busy = false;
    if (s_cli) {
        esp_websocket_client_stop(s_cli);
        esp_websocket_client_destroy(s_cli);
        s_cli = NULL;
    }
    s_rxlen = 0;
    s_task_alive = false;
    vTaskDelete(NULL);
}

/* ==================== 引擎接口 ==================== */

static esp_err_t xf_init(void)
{
    if (!inmp441_is_running()) {
        ESP_LOGE(TAG, "麦克风未就绪，唤醒引擎无法工作");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "引擎就绪: %s (v2 听写)", k_appid());
    return ESP_OK;
}

static esp_err_t xf_start(void)
{
    if (!s_running) {
        s_running    = true;
        s_task_alive = true;
        if (xTaskCreate(session_task, "kws_iat", 8192, NULL, 5, NULL) != pdPASS) {
            s_running    = false;
            s_task_alive = false;
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

static esp_err_t xf_stop(void)
{
    s_running = false;
    for (int i = 0; i < 100 && s_task_alive; i++) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (s_task_alive) {
        ESP_LOGW(TAG, "会话任务 5s 未退出，强制置空闲");
    }
    s_session_busy = false;
    return ESP_OK;
}

static bool xf_busy(void)
{
    return s_session_busy;
}

const kws_engine_t kws_engine_xfyun = {
    .name = "xfyun-iat",
    .init = xf_init,
    .start = xf_start,
    .stop  = xf_stop,
    .busy  = xf_busy,
};
