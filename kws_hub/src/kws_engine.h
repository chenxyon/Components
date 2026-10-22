/**
 * @file     kws_engine.h
 * @brief    唤醒引擎内部接口（组件私有，不对外暴露）
 *
 * 功能：定义各唤醒引擎的统一形态，由 kws_hub 通过引擎表调度。
 *       新增引擎只需实现本接口并在 kws_engine_table 中登记一行。
 * 修改：2026-10-18 新建
 */
#ifndef KWS_ENGINE_H
#define KWS_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  引擎统一形态
 *
 * 说明：引擎自行管理会话与音频读取（从 inmp441 环形缓冲取），
 *       kws_hub 只负责生命周期与事件分发。
 */
typedef struct {
    const char *name;                       /*!< 引擎名，诊断用 */
    esp_err_t (*init)(void);                /*!< 初始化（麦克风/网络/模型）*/
    esp_err_t (*start)(void);               /*!< 开始一个识别会话 */
    esp_err_t (*stop)(void);                /*!< 结束当前会话 */
    bool      (*busy)(void);                /*!< 是否正在识别 */
} kws_engine_t;

/** 引擎产出一段识别结果 */
void kws_engine_emit(const char *text, bool is_final);

/** 引擎报错 */
void kws_engine_emit_error(const char *msg);

extern const kws_engine_t kws_engine_xfyun;
extern const kws_engine_t kws_engine_esp_sr;

#ifdef __cplusplus
}
#endif

#endif /* KWS_ENGINE_H */