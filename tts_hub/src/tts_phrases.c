/* 固定话术数据表
 *
 * 本文件由 tools/gen_tts_clips.py 生成，请勿手改。
 * 当前为占位版本：所有片段为 NULL，运行时自动回退到云 TTS。
 *
 * 生成真实片段：
 *   python tools/gen_tts_clips.py
 * 依赖：edge-tts + ffmpeg（或预先准备好的 wav 目录，见脚本说明）
 */
#include "tts_phrases.h"

const tts_clip_t tts_clips[TTS_PHRASE_MAX] = {
    [TTS_PHRASE_PORTAL_START]    = { NULL, 0, 16000 },
    [TTS_PHRASE_PORTAL_OK]       = { NULL, 0, 16000 },
    [TTS_PHRASE_NET_CONNECTED]   = { NULL, 0, 16000 },
    [TTS_PHRASE_NET_DISCONNECTED]= { NULL, 0, 16000 },
    [TTS_PHRASE_CONNECT_FAILED]  = { NULL, 0, 16000 },
    [TTS_PHRASE_PLEASE_WAIT]     = { NULL, 0, 16000 },
    [TTS_PHRASE_OK]              = { NULL, 0, 16000 },
    [TTS_PHRASE_CANCEL]          = { NULL, 0, 16000 },
    [TTS_PHRASE_PROCESSING]      = { NULL, 0, 16000 },
    [TTS_PHRASE_VOLUME_UP]       = { NULL, 0, 16000 },
    [TTS_PHRASE_VOLUME_DOWN]     = { NULL, 0, 16000 },
};

const char *tts_phrase_text(tts_phrase_t ph)
{
    switch (ph) {
    case TTS_PHRASE_PORTAL_START:     return "配网中，请连接设备热点";
    case TTS_PHRASE_PORTAL_OK:        return "配网成功";
    case TTS_PHRASE_NET_CONNECTED:    return "网络已连接";
    case TTS_PHRASE_NET_DISCONNECTED: return "网络已断开";
    case TTS_PHRASE_CONNECT_FAILED:   return "连接失败，请检查密码";
    case TTS_PHRASE_PLEASE_WAIT:      return "请稍候";
    case TTS_PHRASE_OK:               return "好的";
    case TTS_PHRASE_CANCEL:           return "已取消";
    case TTS_PHRASE_PROCESSING:       return "正在处理";
    case TTS_PHRASE_VOLUME_UP:        return "音量已增大";
    case TTS_PHRASE_VOLUME_DOWN:      return "音量已减小";
    default:                          return "";
    }
}
