/**
 * @file     xfyun_auth.h
 * @brief    讯飞 WebAPI 通用鉴权（HMAC-SHA256 + GMT 时间 + URL 编码）
 *
 * 功能：讯飞语音合成(/v2/tts)、语音听写(/v1)等 WebSocket 服务的鉴权串
 *       生成规则完全一致，抽成独立组件供各厂商驱动复用，避免重复实现。
 * 修改：2026-10-18 新建
 *
 * @note     鉴权流程见讯飞文档「接口鉴权」：
 *           signature_origin = "host: {host}\ndate: {date}\nGET {path} HTTP/1.1"
 *           authorization    = base64('api_key="..",algorithm="hmac-sha256",
 *                                      headers="host date request-line",
 *                                      signature="base64(hmac_sha256)")'
 *           date 与 authorization 均需 URL 编码后拼进 query。
 *
 * @note     服务端对 date 做时钟偏移检查，最大允许 300 秒偏差，
 *           故必须先完成 SNTP 对时再发起请求。
 */
#ifndef XFYUN_AUTH_H
#define XFYUN_AUTH_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  生成带鉴权参数的 WebSocket URL
 *
 * 功能：拼出 host/path + authorization + date + host 四段 query
 * 修改：2026-10-18 新建
 *
 * @param host    请求主机，如 "tts-api.xfyun.cn"
 * @param path    请求路径，如 "/v2/tts"
 * @param api_key 讯飞 API Key
 * @param secret  讯飞 API Secret
 * @param out     输出缓冲；URL 编码后约 1.2KB，建议 >= 2048
 * @param out_n   输出缓冲大小
 *
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数为空；其它为计算失败
 */
esp_err_t xfyun_build_auth_url(const char *host, const char *path,
                               const char *api_key, const char *secret,
                               char *out, size_t out_n);

/**
 * @brief  RFC3986 URL 编码
 *
 * 功能：讯飞鉴权的 date 含空格/逗号/冒号，authorization 的 base64 含
 *       '+' '/' '='，不编码会被 websocket 客户端判为 Invalid uri。
 *       空格按文档示例编成 %20（不是 '+'）。
 * 修改：2026-10-18 新建
 */
void xfyun_url_encode(const char *in, char *out, size_t out_n);

/**
 * @brief  生成当前 GMT 时间字符串
 *
 * 功能：格式 "%a, %d %b %Y %H:%M:%S GMT"，如 "Wed, 01 Oct 2025 12:00:00 GMT"
 * 修改：2026-10-18 新建
 */
void xfyun_gmt_date(char *out, size_t n);

/**
 * @brief  计算 HMAC-SHA256
 *
 * 功能：mbedTLS 4 移除了 mbedtls_md_hmac 一句话版，MD 分步 HMAC 也收为
 *       私有 API，公共路线是 PSA Crypto（import 临时 key + psa_mac_compute）。
 * 修改：2026-10-18 新建
 *
 * @return ESP_OK 成功
 */
esp_err_t xfyun_hmac_sha256(const uint8_t *key, size_t key_len,
                            const uint8_t *msg, size_t msg_len,
                            uint8_t out[32]);

/**
 * @brief  等待 SNTP 对时完成
 *
 * 功能：讯飞鉴权的 date 做时钟偏移检查（最大允许 300 秒偏差），未对时
 *       会握手 403。首次联网 NTP 常需 5~15s，故发起请求前先等待。
 * 修改：2026-10-18 新建
 *
 * @param max_wait_s 最长等待秒数；传 0 用默认值 20
 * @return ESP_OK 时钟已同步；ESP_ERR_TIMEOUT 超时未同步
 */
esp_err_t xfyun_wait_sntp(int max_wait_s);

#ifdef __cplusplus
}
#endif

#endif /* XFYUN_AUTH_H */