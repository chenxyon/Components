/**
 * @file     kws_config.h
 * @brief    唤醒词组件的编译期配置（来自 menuconfig）
 *
 * @note     密钥等具体值在 Kconfig.projbuild 中由项目设置；
 *           本文件只负责映射与兜底，不硬编码任何密钥。
 */
#ifndef KWS_CONFIG_H
#define KWS_CONFIG_H

#include "tts_config.h"   /* 复用 TTS 侧已配置的讯飞密钥 */

#ifdef __cplusplus
extern "C" {
#endif

/* 引擎选择：与 kws_hub.h 的 kws_engine_id_t 对应 */
#define KWS_ENGINE_ID_NONE        0
#define KWS_ENGINE_ID_XFYUN_IAT   1
#define KWS_ENGINE_ID_ESP_SR      2

#if defined(CONFIG_KWS_ENGINE_XFYUN_IAT)
#define CONFIG_KWS_ENGINE_ID   KWS_ENGINE_ID_XFYUN_IAT
#elif defined(CONFIG_KWS_ENGINE_ESP_SR)
#define CONFIG_KWS_ENGINE_ID   KWS_ENGINE_ID_ESP_SR
#else
#define CONFIG_KWS_ENGINE_ID   KWS_ENGINE_ID_XFYUN_IAT
#endif

#ifndef CONFIG_KWS_WAKE_WORD
#define CONFIG_KWS_WAKE_WORD   "小智小智"
#endif

#ifndef CONFIG_KWS_IAT_SAMPLE_RATE
#define CONFIG_KWS_IAT_SAMPLE_RATE  16000
#endif

#ifndef CONFIG_KWS_IAT_EOS_MS
#define CONFIG_KWS_IAT_EOS_MS   2000
#endif

#ifndef CONFIG_KWS_IAT_DOMAIN
#define CONFIG_KWS_IAT_DOMAIN   "lm"
#endif

/* IAT 专用密钥：未配置时回退到 TTS 同一应用的密钥 */
#define ASR_XFYUN_APPID       CONFIG_KWS_ASR_XFYUN_APPID
#define ASR_XFYUN_API_KEY     CONFIG_KWS_ASR_XFYUN_API_KEY
#define ASR_XFYUN_API_SECRET  CONFIG_KWS_ASR_XFYUN_API_SECRET

#ifdef __cplusplus
}
#endif

#endif /* KWS_CONFIG_H */