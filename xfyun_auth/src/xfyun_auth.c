/* 讯飞 WebAPI 通用鉴权实现 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "psa/crypto.h"
#include "mbedtls/base64.h"

#include "xfyun_auth.h"

static const char *TAG = "XFYUN_AUTH";

/* 认为时间已同步的阈值：2023-11-14 之后的 unix 时间戳 */
#define XFYUN_TIME_VALID_AFTER 1700000000L

esp_err_t xfyun_wait_sntp(int max_wait_s)
{
    if (max_wait_s <= 0) max_wait_s = 20;
    for (int i = 0; i < max_wait_s; i++) {
        time_t now = time(NULL);
        if (now > XFYUN_TIME_VALID_AFTER) return ESP_OK;
        ESP_LOGI(TAG, "等待 SNTP 对时… (%d/%d) 当前时间戳=%lld",
                 i + 1, max_wait_s, (long long)now);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGE(TAG, "SNTP %d s 仍未同步，讯飞鉴权很可能失败（时间戳=%lld）",
             max_wait_s, (long long)time(NULL));
    return ESP_ERR_TIMEOUT;
}

void xfyun_gmt_date(char *out, size_t n)
{
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    strftime(out, n, "%a, %d %b %Y %H:%M:%S GMT", &tm);
}

void xfyun_url_encode(const char *in, char *out, size_t out_n)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    if (!in || !out || out_n == 0) return;
    for (size_t i = 0; in[i] && o + 4 < out_n; i++) {
        unsigned char c = (unsigned char)in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0x0F];
        }
    }
    out[o] = '\0';
}

esp_err_t xfyun_hmac_sha256(const uint8_t *key, size_t key_len,
                            const uint8_t *msg, size_t msg_len,
                            uint8_t out[32])
{
    if (psa_crypto_init() != PSA_SUCCESS) return ESP_FAIL;

    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));

    mbedtls_svc_key_id_t key_id;
    if (psa_import_key(&attr, key, key_len, &key_id) != PSA_SUCCESS) return ESP_FAIL;

    size_t mac_len = 0;
    psa_status_t st = psa_mac_compute(key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                                      msg, msg_len, out, 32, &mac_len);
    psa_destroy_key(key_id);
    return (st == PSA_SUCCESS && mac_len == 32) ? ESP_OK : ESP_FAIL;
}

esp_err_t xfyun_build_auth_url(const char *host, const char *path,
                               const char *api_key, const char *secret,
                               char *out, size_t out_n)
{
    if (!host || !path || !api_key || !secret || !out || out_n == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!api_key[0] || !secret[0]) {
        ESP_LOGE(TAG, "API Key/Secret 未配置");
        return ESP_ERR_INVALID_STATE;
    }

    char date[64];
    xfyun_gmt_date(date, sizeof(date));

    char sign_src[256];
    snprintf(sign_src, sizeof(sign_src),
             "host: %s\ndate: %s\nGET %s HTTP/1.1", host, date, path);

    uint8_t hmac[32];
    if (xfyun_hmac_sha256((const uint8_t *)secret, strlen(secret),
                          (const uint8_t *)sign_src, strlen(sign_src),
                          hmac) != ESP_OK) {
        ESP_LOGE(TAG, "HMAC-SHA256 计算失败");
        return ESP_FAIL;
    }

    char sig_b64[64] = {0};
    size_t sig_len = 0;
    mbedtls_base64_encode((unsigned char *)sig_b64, sizeof(sig_b64), &sig_len,
                          hmac, sizeof(hmac));

    char auth_origin[512];
    snprintf(auth_origin, sizeof(auth_origin),
             "api_key=\"%s\", algorithm=\"hmac-sha256\", "
             "headers=\"host date request-line\", signature=\"%s\"",
             api_key, sig_b64);

    char auth_b64[768] = {0};
    size_t auth_len = 0;
    mbedtls_base64_encode((unsigned char *)auth_b64, sizeof(auth_b64), &auth_len,
                          (const unsigned char *)auth_origin,
                          strlen(auth_origin));

    /* 编码后长度约为原长的 3 倍 */
    char auth_enc[2048] = {0};
    char date_enc[128]  = {0};
    xfyun_url_encode(auth_b64, auth_enc, sizeof(auth_enc));
    xfyun_url_encode(date, date_enc, sizeof(date_enc));

    int n = snprintf(out, out_n,
                     "wss://%s%s?authorization=%s&date=%s&host=%s",
                     host, path, auth_enc, date_enc, host);
    if (n < 0 || (size_t)n >= out_n) {
        ESP_LOGE(TAG, "URL 缓冲不足（需 %d 字节，实有 %u）", n, (unsigned)out_n);
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}