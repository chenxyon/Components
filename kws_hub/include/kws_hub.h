/**
 * @file     kws_hub.h
 * @brief    唤醒词检测组件（引擎可插拔）
 *
 * 功能：常驻监听麦克风，把音频喂给「唤醒引擎」；引擎产出识别文字后，
 *       与用户自定义的唤醒词做匹配，命中即回调 KWS_EV_WAKE。
 *       唤醒之后引擎继续识别，后续文字以 KWS_EV_TEXT 上报，可直接当指令用。
 * 修改：2026-10-18 新建
 *
 * @note     引擎通过 KWS_ENGINE_* 选择，业务层无需关心用的是哪种识别方案：
 *           - KWS_ENGINE_XFYUN_IAT：讯飞在线听写 + 文本匹配。
 *             唤醒词完全自定义（改字符串即可），零模型文件零训练；
 *             代价是依赖网络，唤醒延迟约 1~2 秒。
 *           - KWS_ENGINE_ESP_SR：ESP-SR WakeNet 离线唤醒（接口已预留）。
 *             延迟 <100ms 且不耗流量；自定义唤醒词需云端训练模型。
 *
 * @note     音频来源是 inmp441 组件的环形缓冲，采样率须为 16kHz
 *           （讯飞听写与 ESP-SR 唤醒的标准采样率）。
 */
#ifndef KWS_HUB_H
#define KWS_HUB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 唤醒引擎标识（对应 Kconfig 的 KWS_ENGINE 选择）*/
typedef enum {
    KWS_ENGINE_NONE = 0,
    KWS_ENGINE_XFYUN_IAT,   /*!< 讯飞在线听写 + 文本匹配 */
    KWS_ENGINE_ESP_SR,      /*!< ESP-SR WakeNet 离线唤醒（预留） */
} kws_engine_id_t;

/** 唤醒事件类型 */
typedef enum {
    KWS_EV_WAKE  = 0,   /*!< 命中唤醒词，text 为识别到的原文 */
    KWS_EV_TEXT,        /*!< 识别到文字；唤醒后即为指令文本 */
    KWS_EV_ERROR,       /*!< 引擎报错，text 为错误描述 */
} kws_event_t;

/**
 * @brief  唤醒事件回调
 *
 * @param ev   事件类型
 * @param text 识别文本（UTF-8，KWS_EV_ERROR 时为错误描述，可能为 NULL）
 * @param ctx  注册时传入的上下文
 */
typedef void (*kws_event_cb_t)(kws_event_t ev, const char *text, void *ctx);

/**
 * @brief  初始化组件并选定引擎
 *
 * 功能：读取 Kconfig 里的引擎选择，初始化对应引擎；不启动监听
 * 修改：2026-10-18 新建
 *
 * @param engine 引擎标识；传 KWS_ENGINE_NONE 表示按 Kconfig 默认值
 * @return ESP_OK 成功
 */
esp_err_t kws_hub_init(kws_engine_id_t engine);

/**
 * @brief  设置主唤醒词
 *
 * 功能：唤醒匹配的目标词，可随时改成产品自己的叫法
 * 修改：2026-10-18 新建
 *
 * @note     匹配时会忽略识别文本中的空格与标点，
 *           因此「小智小智」「小智， 小智」都能命中。
 */
esp_err_t kws_hub_set_wake_word(const char *word);

/**
 * @brief  追加一个唤醒词别名
 *
 * 功能：用于支持多种叫法，例如主词「小智小智」+ 别名「小智同学」
 * 修改：2026-10-18 新建
 *
 * @param word 别名词，不可为空
 * @return ESP_OK 成功；ESP_ERR_NO_MEM 别名槽位已满
 */
esp_err_t kws_hub_add_wake_alias(const char *word);

/**
 * @brief  取当前主唤醒词
 *
 * 功能：状态查询
 * 修改：2026-10-18 新建
 */
const char *kws_hub_get_wake_word(void);

/**
 * @brief  注册事件回调
 *
 * 功能：唤醒/识别到文字/出错时回调
 * 修改：2026-10-18 新建
 */
esp_err_t kws_hub_set_callback(kws_event_cb_t cb, void *ctx);

/**
 * @brief  开始常驻监听
 *
 * 功能：内部任务持续读麦克风喂给引擎，直到 kws_hub_stop()
 * 修改：2026-10-18 新建
 *
 * @note     监听期间设备有网络流量（在线引擎），可按需功耗管理。
 */
esp_err_t kws_hub_start(void);

/**
 * @brief  停止监听
 *
 * 功能：停止内部任务并断开引擎会话
 * 修改：2026-10-18 新建
 */
esp_err_t kws_hub_stop(void);

/**
 * @brief  暂停监听
 *
 * 功能：TTS 播报期间应暂停，否则麦克风会录到喇叭声音造成自唤醒，
 *       同时也能释放会话内存（唤醒引擎与 TTS 各占一份 TLS 会话）。
 * 修改：2026-10-18 新建
 */
esp_err_t kws_hub_pause(void);

/**
 * @brief  恢复监听
 *
 * 功能：与 kws_hub_pause 配对，播报结束后恢复
 * 修改：2026-10-18 新建
 */
esp_err_t kws_hub_resume(void);

/**
 * @brief  是否正在监听
 *
 * 功能：状态查询
 * 修改：2026-10-18 新建
 */
bool kws_hub_is_listening(void);

/**
 * @brief  取当前引擎名
 *
 * 功能：诊断用，返回如 "xfyun-iat" / "esp-sr"
 * 修改：2026-10-18 新建
 */
const char *kws_hub_engine_name(void);

#ifdef __cplusplus
}
#endif

#endif /* KWS_HUB_H */