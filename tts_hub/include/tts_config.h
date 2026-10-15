#ifndef TTS_CONFIG_H
#define TTS_CONFIG_H

/* ============================================================
 * TTS 统一配置文件 —— 以后换厂商 / 换 Key 只改这里，不改代码
 * ============================================================ */

/* 使用哪个厂商：TTS_PROVIDER_XFYUN / TTS_PROVIDER_BAIDU / TTS_PROVIDER_ALI */
#define TTS_ACTIVE_PROVIDER   TTS_PROVIDER_XFYUN

/* ---------- 科大讯飞 ---------- */
#define TTS_XFYUN_APPID       "5a99f947"
#define TTS_XFYUN_API_KEY     "e542c421f3bdf399222de862f95c9bba"
#define TTS_XFYUN_API_SECRET  "6002f68c3ecf4969ee2313113cad46da"
#define TTS_XFYUN_VCN         "xiaoyan"   /* 发音人 */

/* ---------- 百度智能云（预留，填上即可用） ---------- */
#define TTS_BAIDU_API_KEY     ""
#define TTS_BAIDU_SECRET_KEY  ""

/* ---------- 阿里云智能语音（预留） ---------- */
#define TTS_ALI_APPKEY        ""
#define TTS_ALI_TOKEN         ""
#define TTS_ALI_VOICE         "xiaoyun"

#endif
