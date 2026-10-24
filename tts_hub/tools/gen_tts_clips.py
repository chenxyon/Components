#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_tts_clips.py —— 生成 tts_hub 的离线固定话术片段 (src/tts_phrases.c)

两种用法：
  1) 自动合成（需联网，依赖 edge-tts + ffmpeg）：
       pip install edge-tts
       安装 ffmpeg 并加入 PATH
       python tools/gen_tts_clips.py
  2) 用现成的 wav 文件（离线，任意 TTS 工具导出的 16k/任意采样率单声道 wav）：
       把文件按序号命名放到 ./clips/0.wav 1.wav ...（序号对应下方 PHRASES 顺序）
       python tools/gen_tts_clips.py --wav-dir clips

输出：覆盖 src/tts_phrases.c，把每条话术编码为 s16le 单声道 16kHz PCM，
     为 NULL 的话术运行时会自动回退云 TTS。

注意：片段总大小别超过 app 分区剩余空间（本工程 app0 有 8MB，余量充足）。
"""
import argparse
import base64
import os
import shutil
import subprocess
import sys
import tempfile

# (枚举名, 中文文案, edge-tts 语音)
PHRASES = [
    ("TTS_PHRASE_PORTAL_START",     "配网中，请连接设备热点", "zh-CN-XiaoxiaoNeural"),
    ("TTS_PHRASE_PORTAL_OK",        "配网成功",               "zh-CN-XiaoxiaoNeural"),
    ("TTS_PHRASE_NET_CONNECTED",    "网络已连接",             "zh-CN-XiaoxiaoNeural"),
    ("TTS_PHRASE_NET_DISCONNECTED", "网络已断开",             "zh-CN-XiaoxiaoNeural"),
    ("TTS_PHRASE_CONNECT_FAILED",   "连接失败，请检查密码",   "zh-CN-XiaoxiaoNeural"),
    ("TTS_PHRASE_PLEASE_WAIT",      "请稍候",                 "zh-CN-XiaoxiaoNeural"),
    ("TTS_PHRASE_OK",               "好的",                   "zh-CN-XiaoxiaoNeural"),
    ("TTS_PHRASE_CANCEL",           "已取消",                 "zh-CN-XiaoxiaoNeural"),
    ("TTS_PHRASE_PROCESSING",       "正在处理",               "zh-CN-XiaoxiaoNeural"),
    ("TTS_PHRASE_VOLUME_UP",        "音量已增大",             "zh-CN-XiaoxiaoNeural"),
    ("TTS_PHRASE_VOLUME_DOWN",      "音量已减小",             "zh-CN-XiaoxiaoNeural"),
]

TARGET_RATE = 16000


def have(cmd):
    return shutil.which(cmd) is not None


def synth_edge(text, voice, out_wav):
    """用 edge-tts 合成 mp3，再 ffmpeg 转 s16le 16k 单声道 wav，最后裁成裸 pcm"""
    with tempfile.TemporaryDirectory() as td:
        mp3 = os.path.join(td, "a.mp3")
        subprocess.run(
            ["edge-tts", "--voice", voice, "--text", text, "--write-media", mp3],
            check=True,
        )
        subprocess.run(
            ["ffmpeg", "-y", "-i", mp3, "-f", "wav", "-acodec", "pcm_s16le",
             "-ac", "1", "-ar", str(TARGET_RATE), out_wav],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )


def wav_to_pcm16k(path):
    """把任意 wav 转成 s16le 16k 单声道裸 PCM 字节"""
    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tf:
        tmp = tf.name
    try:
        subprocess.run(
            ["ffmpeg", "-y", "-i", path, "-f", "wav", "-acodec", "pcm_s16le",
             "-ac", "1", "-ar", str(TARGET_RATE), tmp],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        with open(tmp, "rb") as f:
            data = f.read()
        # 跳过 44 字节 wav 头（ffmpeg 输出标准头）
        return data[44:]
    finally:
        try:
            os.unlink(tmp)
        except OSError:
            pass


def c_array(name, data):
    if not data:
        return ""
    lines = [f"static const uint8_t {name}[] = {{"]
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append("    " + ",".join(f"0x{b:02X}" for b in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wav-dir", help="使用现成 wav 的目录（按序号 0.wav..）")
    ap.add_argument("--out", default="src/tts_phrases.c")
    args = ap.parse_args()

    clips = []  # (enum, text, bytes or None)

    if args.wav_dir:
        for idx, (enum, text, _voice) in enumerate(PHRASES):
            wav = os.path.join(args.wav_dir, f"{idx}.wav")
            if os.path.exists(wav):
                clips.append((enum, text, wav_to_pcm16k(wav)))
            else:
                print(f"[warn] 缺少 {wav}，该话术留空", file=sys.stderr)
                clips.append((enum, text, None))
    else:
        if not (have("edge-tts") and have("ffmpeg")):
            print("缺少 edge-tts 或 ffmpeg。请二选一：\n"
                  "  pip install edge-tts 且安装 ffmpeg；\n"
                  "  或改用 --wav-dir 提供现成 wav 文件。", file=sys.stderr)
            sys.exit(1)
        with tempfile.TemporaryDirectory() as td:
            for idx, (enum, text, voice) in enumerate(PHRASES):
                print(f"[synth] {enum}: {text}")
                wav = os.path.join(td, f"{idx}.wav")
                try:
                    synth_edge(text, voice, wav)
                    with open(wav, "rb") as f:
                        data = f.read()[44:]
                    clips.append((enum, text, data))
                except subprocess.CalledProcessError as e:
                    print(f"[warn] 合成失败 {enum}: {e}", file=sys.stderr)
                    clips.append((enum, text, None))

    out = []
    out.append("/* 固定话术数据表 —— 由 tools/gen_tts_clips.py 自动生成，请勿手改 */")
    out.append('#include "tts_phrases.h"')
    out.append("")
    for enum, _text, data in clips:
        arr = c_array(f"clip_{enum}", data)
        if arr:
            out.append(arr)
    out.append("")
    out.append("const tts_clip_t tts_clips[TTS_PHRASE_MAX] = {")
    for enum, _text, data in clips:
        if data:
            out.append(f"    [{enum}] = {{ clip_{enum}, {len(data)}, {TARGET_RATE} }},")
        else:
            out.append(f"    [{enum}] = {{ NULL, 0, {TARGET_RATE} }},")
    out.append("};")
    out.append("")

    # 文案表
    out.append("const char *tts_phrase_text(tts_phrase_t ph)")
    out.append("{")
    out.append("    switch (ph) {")
    for enum, text, _data in clips:
        out.append(f'    case {enum}: return "{text}";')
    out.append('    default: return "";')
    out.append("    }")
    out.append("}")
    out.append("")

    with open(args.out, "w", encoding="utf-8") as f:
        f.write("\n".join(out))

    total = sum(len(d) for _e, _t, d in clips if d)
    print(f"已写出 {args.out}：{sum(1 for *_ ,d in clips if d)}/{len(clips)} 条有音频，"
          f"共 {total} 字节（约 {total/1024:.0f} KB）")


if __name__ == "__main__":
    main()
