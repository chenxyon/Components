/* 唤醒引擎：讯飞在线语音听写（IAT）+ 唤醒词文本匹配
 *
 * 协议见「语音听写」文档：wss://iat.xf-yun.com/v1
 *   首帧带 parameter，之后按 40ms/1280 字节流式发音频，最后一帧 status=2。
 *   返回 payload.result.text 为 base64，解码后是 {sn,ls,bg,ed,ws:[{cw:[{w}]}]}。
 *
 * 这里关闭动态修正(dwa)，只取整句最终结果 —— 唤醒词匹配本来就需要
 * 完整的一句话，中间结果反而会误触发。
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

#define XF_HOST  "iat.xf-yun.com"
#define XF_PATH  "/v1"

/* 讯飞听写推荐：每 40ms 发 1280 字节（=640 采样@16kHz，正好实时）*/
#define IAT_CHUNK_BYTES   1280
#define IAT_CHUNK_MS      40
#define IAT_RX_BUF        4096   /* 听写返回报文很小（关动态修正），够用 */

/* 引擎会话状态 */
static volatile bool s_running;
static volatile bool s_session_busy;
/* 会话任务是否存活（含句柄清理）。xf_stop 必须等它转为 false 才能放行，
 * 否则新会话会覆盖尚未销毁的 s_cli 造成重复销毁/野指针。 */
static volatile bool s_task_alive;
/* 建连失败标志：让 session_task 立刻放弃等待，而不是傻等 10s */
static volatile bool s_ws_error;
static esp_websocket_client_handle_t s_cli;
static char s_rx[IAT_RX_BUF];
static size_t s_rxlen;
static int s_seq;

/* ==================== 发送 ==================== */

/* 取 appid/key/secret：ASR 未单独配置时回退到 TTS 同一应用的密钥
 * （讯飞的应用级密钥对该应用下所有服务通用）。*/
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

/* 发一帧音频。status: 0=首帧(带 parameter) 1=中间 2=结束 */
static void send_audio_frame(const uint8_t *pcm, size_t len, int status)
{
    static char b64[IAT_CHUNK_BYTES / 3 * 4 + 8];
    char *js;
    size_t olen = 0;

    b64[0] = '\0';
    if (len) {
        mbedtls_base64_encode((unsigned char *)b64, sizeof(b64), &olen,
                              pcm, len);
        b64[olen] = '\0';
    }

    /* 首帧才带识别参数 */
    if (status == 0) {
        js = malloc(1024 + olen);
        if (!js) return;
        snprintf(js, 1024 + olen,
                 "{\"header\":{\"app_id\":\"%s\",\"status\":0},"
                 "\"parameter\":{\"iat\":{\"domain\":\"%s\",\"language\":\"zh_cn\","
                 "\"accent\":\"mandarin\",\"eos\":%d,\"dwa\":\"wpgs\","
                 "\"result\":{\"encoding\":\"utf8\",\"compress\":\"raw\","
                 "\"format\":\"json\"}}},"
                 "\"payload\":{\"audio\":{\"encoding\":\"raw\",\"sample_rate\":%u,"
                 "\"channels\":1,\"bit_depth\":16,\"seq\":%d,\"status\":0,"
                 "\"audio\":\"%s\"}}}",
                 k_appid(), CONFIG_KWS_IAT_DOMAIN, CONFIG_KWS_IAT_EOS_MS,
                 (unsigned)CONFIG_KWS_IAT_SAMPLE_RATE, s_seq, b64);
    } else {
        js = malloc(384 + olen);
        if (!js) return;
        snprintf(js, 384 + olen,
                 "{\"header\":{\"app_id\":\"%s\",\"status\":%d},"
                 "\"payload\":{\"audio\":{\"encoding\":\"raw\",\"sample_rate\":%u,"
                 "\"channels\":1,\"bit_depth\":16,\"seq\":%d,\"status\":%d,"
                 "\"audio\":\"%s\"}}}",
                 k_appid(), status, (unsigned)CONFIG_KWS_IAT_SAMPLE_RATE,
                 s_seq, status, b64);
    }

    esp_websocket_client_send_text(s_cli, js, strlen(js), pdMS_TO_TICKS(200));
    free(js);
    s_seq++;
}

/* ==================== 接收解析 ==================== */

/* 把听写 result.text 解出的分词拼成整句 */
static char *join_words(cJSON *result)
{
    cJSON *ws = cJSON_GetObjectItem(result, "ws");
    cJSON *seg = cJSON_GetArrayItem(ws, 0);
    cJSON *cw = cJSON_GetObjectItem(seg, "cw");
    if (!cw) return NULL;

    int n = (int)cJSON_GetArraySize(cw);
    if (n <= 0) return NULL;

    size_t total = 1;
    for (int i = 0; i < n; i++) {
        cJSON *w = cJSON_GetObjectItem(cJSON_GetArrayItem(cw, i), "w");
        if (w && w->valuestring) total += strlen(w->valuestring);
    }
    char *out = malloc(total);
    if (!out) return NULL;
    out[0] = '\0';
    size_t off = 0;
    for (int i = 0; i < n; i++) {
        cJSON *w = cJSON_GetObjectItem(cJSON_GetArrayItem(cw, i), "w");
        if (w && w->valuestring) {
            size_t l = strlen(w->valuestring);
            memcpy(out + off, w->valuestring, l);
            off += l;
            out[off] = '\0';
        }
    }
    return out;
}

static void handle_response(const char *json, size_t len)
{
    cJSON *r = cJSON_ParseWithLength(json, len);
    if (!r) return;

    cJSON *code = cJSON_GetObjectItem(r, "code");
    int c = code ? code->valueint : -1;
    if (c != 0) {
        cJSON *sid  = cJSON_GetObjectItem(r, "sid");
        cJSON *msg  = cJSON_GetObjectItem(r, "message");
        cJSON *stat = cJSON_GetObjectItem(r, "status");
        char buf[128];
        snprintf(buf, sizeof(buf), "code=%d sid=%s status=%s msg=%s",
                 c,
                 (sid && sid->valuestring) ? sid->valuestring : "-",
                 stat && stat->valueint ? "非0" : "0",
                 (msg && msg->valuestring && msg->valuestring[0])
                     ? msg->valuestring : "(无)");
        kws_engine_emit_error(buf);
        s_session_busy = false;
        cJSON_Delete(r);
        return;
    }

    cJSON *pl = cJSON_GetObjectItem(r, "payload");
    if (!pl) { cJSON_Delete(r); return; }

    /* data 可能为 null 且 code=0（文档注意事项 1），直接忽略 */
    cJSON *res = cJSON_GetObjectItem(pl, "result");
    if (!res) { cJSON_Delete(r); return; }

    cJSON *txt = cJSON_GetObjectItem(res, "text");
    if (txt && txt->valuestring && txt->valuestring[0]) {
        size_t tl = strlen(txt->valuestring);
        uint8_t *dec = malloc(tl + 4);
        size_t dlen = 0;
        if (dec && mbedtls_base64_decode(dec, tl + 4, &dlen,
                (const unsigned char *)txt->valuestring, tl) == 0) {
            cJSON *inner = cJSON_ParseWithLength((const char *)dec, dlen);
            if (inner) {
                char *text = join_words(inner);
                if (text && text[0]) {
                    cJSON *ls = cJSON_GetObjectItem(inner, "ls");
                    bool final = ls && ls->valueint;
                    ESP_LOGD(TAG, "识别: %s (final=%d)", text, (int)final);
                    kws_engine_emit(text, final);
                }
                free(text);
                cJSON_Delete(inner);
            }
        }
        free(dec);
    }

    /* ls=true 表示本次会话结果结束 */
    cJSON *ls = cJSON_GetObjectItem(res, "ls");
    if (ls && ls->valueint) s_session_busy = false;

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
        /* 听写返回报文较小，采用「追加到能解析为止」策略：
         * 分片没收全就 cJSON 解析失败，等下一片再来。*/
        if (s_rxlen + ev->data_len >= sizeof(s_rx)) {
            ESP_LOGW(TAG, "听写返回报文超长(%u)，丢弃", (unsigned)ev->data_len);
            s_rxlen = 0;
            break;
        }
        memcpy(s_rx + s_rxlen, ev->data_ptr, ev->data_len);
        s_rxlen += ev->data_len;

        /* 只有能解析成完整 JSON 才处理，否则等后续分片 */
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

/**
 * @brief  一个识别会话：连上后按 40ms/1280B 实时推麦克风数据，直到引擎收尾
 */
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
    s_ws_error = false;

    /* TLS/PSA 的工作缓冲走**内部 RAM**，PSRAM 帮不上。
     * 所以这里同时打印内部 RAM 剩余，才是判断 TLS 能否建连的关键指标。 */
    ESP_LOGI(TAG, "建连前内存: 总剩=%u 最大连续块=%u | 内部RAM剩=%u 内部最大块=%u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    esp_websocket_client_config_t cfg = {
        .uri = url,
        .buffer_size = 2048,
        .task_stack = 4096,
        /* 重连由 kws_hub 的 supervisor 统一调度。
         * 开客户端自身的自动重连会和 supervisor 的 stop/destroy 打架，
         * 出现「Did not get TCP close within expected delay」并抢 TLS 内存。 */
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

    for (int i = 0; i < 100 && s_running; i++) {   /* 等连接最多 10s */
        if (s_session_busy) { connected = true; break; }
        if (s_ws_error) break;                     /* 建连失败，立即退出 */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!connected) {
        ESP_LOGW(TAG, "听写连接失败/超时");
        goto finish;
    }

    ESP_LOGI(TAG, "开始流式识别（%u ms/%u 字节）", (unsigned)IAT_CHUNK_MS,
             (unsigned)IAT_CHUNK_BYTES);
    send_audio_frame(NULL, 0, 0);     /* 首帧：带 parameter，音频可为空 */

    /* 会话最长 55s（讯飞上限 60s），留余量后主动收尾并重开会话 */
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
            break;                       /* 麦克风无数据，放弃本会话 */
        }
        /* 按固定节拍推流，避免 send 耗时导致节奏漂移 */
        vTaskDelay(pdMS_TO_TICKS(IAT_CHUNK_MS));
    }

    if (s_running) {
        send_audio_frame(NULL, 0, 2);   /* 最后一帧：声明结束 */
        /* 等收尾结果，最多 1.5s */
        for (int i = 0; i < 15 && s_session_busy; i++) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

finish:
    /* 所有退出路径都汇聚到这里：先收尾会话，再销毁句柄，
     * 最后才清 s_task_alive —— 保证 xf_stop() 返回时句柄已真正释放。 */
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
    ESP_LOGI(TAG, "引擎就绪: %s", k_appid());
    return ESP_OK;
}

static esp_err_t xf_start(void)
{
    if (!s_running) {
        s_running    = true;
        s_task_alive = true;   /* 先置位再建任务，避免 xf_stop 提前判定已退出 */
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
    /* 等会话任务真正退出（含 s_cli 销毁），最多 5s */
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