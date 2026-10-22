#ifndef TTS_CONFIG_H
#define TTS_CONFIG_H

/* ============================================================
 * TTS 统一配置文件 —— 组件只声明“变量名”，具体值由使用它的项目提供。
 * 这样组件里不放任何密钥，换 Key / 换项目值只在项目里改。
 * ============================================================ */

/* 使用哪个厂商：TTS_PROVIDER_XFYUN / TTS_PROVIDER_BAIDU / TTS_PROVIDER_ALI */
#define TTS_ACTIVE_PROVIDER   TTS_PROVIDER_XFYUN

/* ---------- 科大讯飞 ----------
 * 鉴权三项由使用它的项目提供，走 Kconfig（menuconfig → TTS Hub，或
 * 直接写项目 sdkconfig）。组件只声明“变量名”（CONFIG_TTS_XFYUN_*），
 * 值进项目自己的 sdkconfig，组件源码不落任何密钥。
 * 未配置时为空字符串，运行时鉴权失败会在日志里报 E。 */
#ifndef CONFIG_TTS_XFYUN_APPID
#define CONFIG_TTS_XFYUN_APPID ""
#endif
#ifndef CONFIG_TTS_XFYUN_API_KEY
#define CONFIG_TTS_XFYUN_API_KEY ""
#endif
#ifndef CONFIG_TTS_XFYUN_API_SECRET
#define CONFIG_TTS_XFYUN_API_SECRET ""
#endif
#define TTS_XFYUN_APPID       CONFIG_TTS_XFYUN_APPID       /* 项目定义值 */
#define TTS_XFYUN_API_KEY     CONFIG_TTS_XFYUN_API_KEY     /* 项目定义值 */
#define TTS_XFYUN_API_SECRET  CONFIG_TTS_XFYUN_API_SECRET  /* 项目定义值 */
#define TTS_XFYUN_VCN         "xiaoyan"   /* 发音人（非密钥，留组件内） */

/* 语速/音量：讯飞 business 字段，取值 0-100（默认 50，越小越慢）。
 * 值由项目 sdkconfig 提供（menuconfig → TTS Hub）。 */
#ifndef CONFIG_TTS_XFYUN_SPEED
#define CONFIG_TTS_XFYUN_SPEED 40
#endif
#ifndef CONFIG_TTS_XFYUN_VOLUME
#define CONFIG_TTS_XFYUN_VOLUME 50
#endif
#define TTS_XFYUN_SPEED        CONFIG_TTS_XFYUN_SPEED        /* 项目定义值 */
#define TTS_XFYUN_VOLUME       CONFIG_TTS_XFYUN_VOLUME       /* 项目定义值 */

/* ---------- 百度智能云（预留，项目定义变量后即可用） ---------- */
#ifndef CONFIG_TTS_BAIDU_API_KEY
#define CONFIG_TTS_BAIDU_API_KEY ""
#endif
#ifndef CONFIG_TTS_BAIDU_SECRET_KEY
#define CONFIG_TTS_BAIDU_SECRET_KEY ""
#endif
#define TTS_BAIDU_API_KEY     CONFIG_TTS_BAIDU_API_KEY      /* 项目定义值 */
#define TTS_BAIDU_SECRET_KEY  CONFIG_TTS_BAIDU_SECRET_KEY   /* 项目定义值 */

/* ---------- 阿里云智能语音（预留） ---------- */
#ifndef CONFIG_TTS_ALI_APPKEY
#define CONFIG_TTS_ALI_APPKEY ""
#endif
#ifndef CONFIG_TTS_ALI_TOKEN
#define CONFIG_TTS_ALI_TOKEN ""
#endif
#define TTS_ALI_APPKEY        CONFIG_TTS_ALI_APPKEY         /* 项目定义值 */
#define TTS_ALI_TOKEN         CONFIG_TTS_ALI_TOKEN          /* 项目定义值 */
#define TTS_ALI_VOICE         "xiaoyun"

#endif
