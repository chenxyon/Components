/* 科大讯飞 WebSocket TTS 驱动
 * 鉴权：HMAC-SHA256，GMT 时间，wss://tts-api.xfyun.cn/v2/tts
 * 返回 aue=raw（16-bit PCM 16000Hz），经回调原样上抛，
 * 重采样由上层（播放侧）按喇叭采样率统一处理。
 */
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_websocket_client.h"
#include "psa/crypto.h"
#include "mbedtls/base64.h"
#include "cJSON.h"

#include "tts_hub.h"
#include "tts_config.h"
#include "tts_xfyun.h"

static const char *TAG = "TTS_XFYUN";

#define XF_HOST     "tts-api.xfyun.cn"
#define XF_PATH     "/v2/tts"
#define XF_URL_BASE "wss://tts-api.xfyun.cn/v2/tts"

typedef struct {
    const char   *text;   /* 待合成 UTF-8 文本 */
    tts_pcm_cb_t  cb;
    void         *uctx;   /* 回调用户上下文 */
    SemaphoreHandle_t done;
    volatile bool failed;
    esp_websocket_client_handle_t cli;
} xfyun_job_t;

static volatile bool s_busy = false;

/* GMT 时间：Wed, 01 Oct 2025 12:00:00 GMT */
static void gmt_date(char *out, size_t n)
{
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    strftime(out, n, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

/* HMAC-SHA256。
 * mbedTLS 4 移除了 mbedtls_md_hmac 一句话版，MD 分步 HMAC 也收为私有 API，
 * 公共路线是 PSA Crypto（import 临时 HMAC key + psa_mac_compute 单发计算）。 */
static int hmac_sha256(const uint8_t *key, size_t key_len,
                       const uint8_t *msg, size_t msg_len,
                       uint8_t out[32])
{
    if (psa_crypto_init() != PSA_SUCCESS) return -1;

    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));

    mbedtls_svc_key_id_t key_id;
    if (psa_import_key(&attr, key, key_len, &key_id) != PSA_SUCCESS) return -1;

    size_t mac_len = 0;
    psa_status_t st = psa_mac_compute(key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                                      msg, msg_len, out, 32, &mac_len);
    psa_destroy_key(key_id);
    return (st == PSA_SUCCESS && mac_len == 32) ? 0 : -1;
}

static void build_auth_url(char *out, size_t out_n)
{
    char date[64];
    gmt_date(date, sizeof(date));

    char sign_src[256];
    snprintf(sign_src, sizeof(sign_src),
             "host: " XF_HOST "\ndate: %s\nGET " XF_PATH " HTTP/1.1", date);

    unsigned char hmac[32];
    if (hmac_sha256((const uint8_t *)TTS_XFYUN_API_SECRET,
                    strlen(TTS_XFYUN_API_SECRET),
                    (const uint8_t *)sign_src, strlen(sign_src), hmac) != 0) {
        ESP_LOGE(TAG, "HMAC-SHA256 计算失败");
        out[0] = '\0';
        return;
    }

    char sig_b64[64] = {0};
    size_t sig_len = 0;
    mbedtls_base64_encode((unsigned char *)sig_b64, sizeof(sig_b64), &sig_len,
                          hmac, sizeof(hmac));

    char auth_origin[512];
    snprintf(auth_origin, sizeof(auth_origin),
             "api_key=\"%s\", algorithm=\"hmac-sha256\", "
             "headers=\"host date request-line\", signature=\"%s\"",
             TTS_XFYUN_API_KEY, sig_b64);

    char auth_b64[768] = {0};
    size_t auth_len = 0;
    mbedtls_base64_encode((unsigned char *)auth_b64, sizeof(auth_b64), &auth_len,
                          (const unsigned char *)auth_origin,
                          strlen(auth_origin));

    snprintf(out, out_n, XF_URL_BASE
             "?authorization=%s&date=%s&host=" XF_HOST, auth_b64, date);
}

static void ws_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    xfyun_job_t *job = (xfyun_job_t *)arg;
    esp_websocket_event_data_t *ev = data;
    (void)base;
    switch (id) {
    case WEBSOCKET_EVENT_CONNECTED: {
        ESP_LOGI(TAG, "已连接，发送文本");
        cJSON *root = cJSON_CreateObject();
        cJSON *common = cJSON_CreateObject();
        cJSON_AddStringToObject(common, "app_id", TTS_XFYUN_APPID);
        cJSON_AddItemToObject(root, "common", common);
        cJSON *bus = cJSON_CreateObject();
        cJSON_AddStringToObject(bus, "aue", "raw");
        cJSON_AddStringToObject(bus, "auf", "audio/L16;rate=16000");
        cJSON_AddStringToObject(bus, "vcn", TTS_XFYUN_VCN);
        cJSON_AddStringToObject(bus, "tte", "UTF8");
        cJSON_AddItemToObject(root, "business", bus);
        /* 文本需先做 UTF-8 Base64（讯飞要求） */
        size_t tlen = strlen(job->text);
        size_t blen = (tlen + 2) / 3 * 4 + 1;
        char *b64 = malloc(blen);
        size_t olen = 0;
        mbedtls_base64_encode((unsigned char *)b64, blen, &olen,
                              (const unsigned char *)job->text, tlen);
        cJSON *dt = cJSON_CreateObject();
        cJSON_AddNumberToObject(dt, "status", 2);
        cJSON_AddStringToObject(dt, "text", b64);
        free(b64);
        cJSON_AddItemToObject(root, "data", dt);
        char *js = cJSON_PrintUnformatted(root);
        esp_websocket_client_send_text(job->cli, js, strlen(js), portMAX_DELAY);
        cJSON_free(js);
        cJSON_Delete(root);
        break;
    }
    case WEBSOCKET_EVENT_DATA: {
        cJSON *r = cJSON_ParseWithLength(ev->data_ptr, ev->data_len);
        if (!r) break;
        cJSON *code = cJSON_GetObjectItem(r, "code");
        if (!code || code->valueint != 0) {
            cJSON *msg = cJSON_GetObjectItem(r, "message");
            ESP_LOGE(TAG, "错误 code=%d msg=%s",
                     code ? code->valueint : -1,
                     msg ? msg->valuestring : "");
            job->failed = true;
            cJSON_Delete(r);
            xSemaphoreGive(job->done);
            break;
        }
        cJSON *d = cJSON_GetObjectItem(r, "data");
        if (d) {
            cJSON *a = cJSON_GetObjectItem(d, "audio");
            if (a && a->valuestring && job->cb) {
                size_t in_len = strlen(a->valuestring);
                size_t need = in_len * 3 / 4 + 4;
                uint8_t *raw = malloc(need);
                if (raw) {
                    size_t olen = 0;
                    if (mbedtls_base64_decode(raw, need, &olen,
                            (const unsigned char *)a->valuestring,
                            in_len) == 0 && olen >= 2) {
                        job->cb((int16_t *)raw, olen / 2, 16000, job->uctx);
                    }
                    free(raw);
                }
            }
            cJSON *st = cJSON_GetObjectItem(d, "status");
            if (st && st->valueint == 2) xSemaphoreGive(job->done);
        }
        cJSON_Delete(r);
        break;
    }
    case WEBSOCKET_EVENT_ERROR:
    case WEBSOCKET_EVENT_CLOSED:
    case WEBSOCKET_EVENT_DISCONNECTED:
        job->failed = true;
        xSemaphoreGive(job->done);
        break;
    default:
        break;
    }
}

static esp_err_t xfyun_init(void)
{
    return ESP_OK;
}

static bool xfyun_busy(void)
{
    return s_busy;
}

static esp_err_t xfyun_speak(const char *text, tts_pcm_cb_t cb, void *ctx)
{
    if (s_busy) return ESP_ERR_INVALID_STATE;
    if (!text || !text[0]) return ESP_ERR_INVALID_ARG;
    s_busy = true;

    /* 等 SNTP 对时，否则 GMT 鉴权必失败 */
    for (int i = 0; i < 5; i++) {
        if (time(NULL) > 1700000000) break;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    xfyun_job_t job = {
        .text = text,
        .cb = cb,
        .uctx = ctx,
        .done = xSemaphoreCreateBinary(),
        .failed = false,
        .cli = NULL,
    };

    char url[1400];
    build_auth_url(url, sizeof(url));

    esp_websocket_client_config_t cfg = {
        .uri = url,
        .buffer_size = 8192,
        .task_stack = 6144,
        .disable_auto_reconnect = true,
    };
    job.cli = esp_websocket_client_init(&cfg);
    esp_websocket_register_events(job.cli, WEBSOCKET_EVENT_ANY, ws_event, &job);
    esp_websocket_client_start(job.cli);

    bool ok = (xSemaphoreTake(job.done, pdMS_TO_TICKS(30000)) == pdTRUE)
              && !job.failed;
    esp_websocket_client_stop(job.cli);
    esp_websocket_client_destroy(job.cli);
    vSemaphoreDelete(job.done);
    s_busy = false;
    return ok ? ESP_OK : ESP_FAIL;
}

const tts_driver_t tts_drv_xfyun = {
    .name = "xfyun",
    .init = xfyun_init,
    .speak = xfyun_speak,
    .is_busy = xfyun_busy,
};
