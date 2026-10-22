/* 唤醒词检测中枢：引擎调度 + 唤醒词匹配
 *
 * 职责边界：
 *   - 本文件只管「选谁干活 / 拿到文字后判断是不是唤醒 / 事件分发」
 *   - 具体识别由 kws_engine_* 完成（讯飞在线听写 / ESP-SR 离线）
 */
#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "kws_hub.h"
#include "kws_config.h"
#include "kws_engine.h"

static const char *TAG = "KWS_HUB";

#define KWS_MAX_ALIAS  4       /* 主唤醒词之外最多几个别名 */
#define KWS_TEXT_MAX   256

/* ==================== 状态 ==================== */

static const kws_engine_t *s_engine;
static kws_event_cb_t  s_cb;
static void            *s_cb_ctx;

static char  s_wake[KWS_TEXT_MAX];
static char  s_alias[KWS_MAX_ALIAS][KWS_TEXT_MAX];
static int   s_alias_num;

static volatile bool s_listening;
static volatile bool s_paused;       /* TTS 播报期间暂停，避免录到喇叭声音 */
static bool s_sup_created;    /* 监督任务是否已创建（init 可重复调用） */
static bool s_wake_fired;     /* 已唤醒，等待后续指令文本 */

/* ==================== 唤醒词归一化与匹配 ==================== */

/* 去掉空格与常见标点，让「小智， 小智」也能命中「小智小智」。
 * 中文按 UTF-8 原样保留，只删 ASCII 空白与标点。 */
static void normalize(const char *in, char *out, size_t out_n)
{
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < out_n; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == ',' || c == '.' || c == '?' || c == '!' ||
            c == '~' || c == '-' || c == '_' || c == ':' || c == ';') {
            continue;
        }
        out[o++] = (char)c;
    }
    out[o] = '\0';
}

/* 命中则返回命中的那个词，否则 NULL */
static const char *match_wake(const char *norm_text)
{
    const char *hit = NULL;
    if (s_wake[0] && strstr(norm_text, s_wake)) hit = s_wake;
    for (int i = 0; i < s_alias_num && !hit; i++) {
        if (s_alias[i][0] && strstr(norm_text, s_alias[i])) hit = s_alias[i];
    }
    return hit;
}
/* ==================== 引擎回调 ==================== */

void kws_engine_emit(const char *text, bool is_final)
{
    if (!text || !text[0]) return;

    if (s_wake_fired) {
        /* 已唤醒：这段文字当指令上报 */
        ESP_LOGI(TAG, "指令文本: %s", text);
        if (s_cb) s_cb(KWS_EV_TEXT, text, s_cb_ctx);
        /* 一次唤醒只取第一句指令，之后回到等待唤醒 */
        s_wake_fired = false;
        return;
    }

    if (!is_final) return;   /* 只用整句结果判断，避免中间态误触发 */

    char norm[KWS_TEXT_MAX];
    normalize(text, norm, sizeof(norm));
    const char *hit = match_wake(norm);
    if (!hit) return;

    ESP_LOGI(TAG, "命中唤醒词 [%s]：%s", hit, text);
    s_wake_fired = true;
    if (s_cb) s_cb(KWS_EV_WAKE, text, s_cb_ctx);
}

void kws_engine_emit_error(const char *msg)
{
    ESP_LOGE(TAG, "引擎错误: %s", msg ? msg : "?");
    if (s_cb) s_cb(KWS_EV_ERROR, msg, s_cb_ctx);
}

/* ==================== 监督任务 ==================== */

/* 常驻任务（init 时创建一次，永不退出）。
 *
 * 设计要点：start/stop 只翻标志位、立即返回，绝不阻塞调用者。
 * 真正的引擎建连/收尾（关 TLS 可能要数秒）全在这里做 ——
 * 因为 stop() 可能从 WiFi 事件任务里调用，绝不能让事件循环干等。 */
static void supervisor_task(void *arg)
{
    (void)arg;
    for (;;) {
        /* 不该监听（已停/暂停）时，确保会话已关闭 */
        if (!s_listening || s_paused) {
            if (s_engine->busy()) s_engine->stop();
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (s_engine->start() != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        while (s_listening && !s_paused && s_engine->busy()) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        if (!s_listening || s_paused) {
            s_engine->stop();       /* 主动收尾，释放 TLS/websocket 内存 */
        } else {
            vTaskDelay(pdMS_TO_TICKS(300));  /* 避免频繁重连打爆服务端 */
        }
    }
}

esp_err_t kws_hub_pause(void)
{
    if (!s_listening || s_paused) return ESP_OK;
    s_paused = true;
    ESP_LOGI(TAG, "暂停监听（TTS 播报中）");
    return ESP_OK;
}

esp_err_t kws_hub_resume(void)
{
    if (!s_paused) return ESP_OK;
    s_paused = false;
    ESP_LOGI(TAG, "恢复监听");
    return ESP_OK;
}
/* ==================== 对外接口 ==================== */

esp_err_t kws_hub_init(kws_engine_id_t engine)
{
    kws_engine_id_t id = engine;
    if (id == KWS_ENGINE_NONE) {
        id = CONFIG_KWS_ENGINE_ID;   /* Kconfig 默认 */
    }
    switch (id) {
    case KWS_ENGINE_XFYUN_IAT: s_engine = &kws_engine_xfyun; break;
    case KWS_ENGINE_ESP_SR:    s_engine = &kws_engine_esp_sr; break;
    default:
        ESP_LOGE(TAG, "未知唤醒引擎 id=%d", (int)id);
        return ESP_ERR_INVALID_ARG;
    }

    /* 默认唤醒词取自 Kconfig，运行时可再改 */
    if (!s_wake[0]) {
        const char *def = CONFIG_KWS_WAKE_WORD;
        strncpy(s_wake, def[0] ? def : "xiaozhi", sizeof(s_wake) - 1);
        s_wake[sizeof(s_wake) - 1] = '\0';
    }
    normalize(s_wake, s_wake, sizeof(s_wake));   /* 归一化后存回 */

    esp_err_t err = s_engine->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "引擎 %s 初始化失败: %s", s_engine->name,
                 esp_err_to_name(err));
        s_engine = NULL;
        return err;
    }

    /* 监督任务常驻：只创建一次（init 可能因 WiFi 重连被重复调用） */
    if (!s_sup_created) {
        if (xTaskCreate(supervisor_task, "kws_sup", 4096, NULL, 4, NULL) != pdPASS) {
            ESP_LOGE(TAG, "监督任务创建失败");
            s_engine = NULL;
            return ESP_ERR_NO_MEM;
        }
        s_sup_created = true;
    }
    ESP_LOGI(TAG, "初始化完成，引擎=%s，唤醒词=%s", s_engine->name, s_wake);
    return ESP_OK;
}

esp_err_t kws_hub_set_wake_word(const char *word)
{
    if (!word || !word[0]) return ESP_ERR_INVALID_ARG;
    char norm[KWS_TEXT_MAX];
    normalize(word, norm, sizeof(norm));
    if (!norm[0]) return ESP_ERR_INVALID_ARG;
    strncpy(s_wake, norm, sizeof(s_wake) - 1);
    s_wake[sizeof(s_wake) - 1] = '\0';
    ESP_LOGI(TAG, "唤醒词设为: %s", s_wake);
    return ESP_OK;
}

esp_err_t kws_hub_add_wake_alias(const char *word)
{
    if (!word || !word[0]) return ESP_ERR_INVALID_ARG;
    if (s_alias_num >= KWS_MAX_ALIAS) return ESP_ERR_NO_MEM;
    char norm[KWS_TEXT_MAX];
    normalize(word, norm, sizeof(norm));
    if (!norm[0]) return ESP_ERR_INVALID_ARG;
    strncpy(s_alias[s_alias_num], norm, sizeof(s_alias[0]) - 1);
    s_alias[s_alias_num][sizeof(s_alias[0]) - 1] = '\0';
    ESP_LOGI(TAG, "别名[%d] += %s", s_alias_num, s_alias[s_alias_num]);
    s_alias_num++;
    return ESP_OK;
}

const char *kws_hub_get_wake_word(void)
{
    return s_wake;
}

esp_err_t kws_hub_set_callback(kws_event_cb_t cb, void *ctx)
{
    s_cb     = cb;
    s_cb_ctx = ctx;
    return ESP_OK;
}

esp_err_t kws_hub_start(void)
{
    if (!s_engine) return ESP_ERR_INVALID_STATE;
    s_wake_fired = false;
    s_paused     = false;
    s_listening  = true;      /* 监督任务会自动建会话，本函数不阻塞 */
    ESP_LOGI(TAG, "开始监听唤醒词");
    return ESP_OK;
}

esp_err_t kws_hub_stop(void)
{
    /* 只置标志，立即返回；会话收尾由监督任务异步完成。
     * 这样从 WiFi 事件回调里调用也不会阻塞事件循环。 */
    if (s_listening) ESP_LOGI(TAG, "停止监听唤醒词");
    s_listening = false;
    return ESP_OK;
}

bool kws_hub_is_listening(void)
{
    return s_listening;
}

const char *kws_hub_engine_name(void)
{
    return s_engine ? s_engine->name : "none";
}