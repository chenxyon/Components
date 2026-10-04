#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import argparse

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Error: PIL/Pillow not installed. Please install with: pip install pillow")
    sys.exit(1)

FONT_WIDTH = 16
FONT_HEIGHT = 16
FONT_BYTES = FONT_WIDTH * FONT_HEIGHT // 8

UNICODE_CJK_START = 0x4E00
UNICODE_CJK_END = 0x9FA5
UNICODE_CJK_COUNT = UNICODE_CJK_END - UNICODE_CJK_START + 1

def generate_font_data(font_path, font_size=16):
    try:
        font = ImageFont.truetype(font_path, font_size)
    except IOError:
        print(f"Error: Cannot load font from {font_path}")
        print("Trying default font...")
        font = ImageFont.load_default()
    
    font_data = bytearray(UNICODE_CJK_COUNT * FONT_BYTES)
    unicode_to_gbk = bytearray(UNICODE_CJK_COUNT * 2)
    valid_count = 0
    
    for high in range(0x81, 0xFF + 1):
        if high == 0x80:
            continue
        
        if high <= 0xA0:
            low_start = 0x40
            low_end = 0xFE
        elif high <= 0xA9:
            low_start = 0xA1
            low_end = 0xFE
        elif high == 0xAA:
            low_start = 0xA1
            low_end = 0xFE
        elif high <= 0xF7:
            low_start = 0x40
            low_end = 0xFE
        else:
            low_start = 0xA1
            low_end = 0xFE
        
        for low in range(low_start, low_end + 1):
            if (high == 0xA8 and low == 0xA0) or (high == 0xF8 and low == 0xA0):
                continue
            
            gbk_code = (high << 8) | low
            
            try:
                char = bytes([high, low]).decode('gbk')
                if len(char) == 1:
                    unicode_val = ord(char)
                    if UNICODE_CJK_START <= unicode_val <= UNICODE_CJK_END:
                        idx = unicode_val - UNICODE_CJK_START
                        unicode_to_gbk[idx * 2] = (gbk_code >> 8) & 0xFF
                        unicode_to_gbk[idx * 2 + 1] = gbk_code & 0xFF
                        valid_count += 1
            except:
                continue
    
    for unicode_val in range(UNICODE_CJK_START, UNICODE_CJK_END + 1):
        idx = unicode_val - UNICODE_CJK_START
        gbk_code = (unicode_to_gbk[idx * 2] << 8) | unicode_to_gbk[idx * 2 + 1]
        
        if gbk_code == 0:
            continue
        
        try:
            char = bytes([gbk_code >> 8, gbk_code & 0xFF]).decode('gbk')
        except:
            continue
        
        img = Image.new('1', (FONT_WIDTH, FONT_HEIGHT), 0)
        draw = ImageDraw.Draw(img)
        
        draw.text((0, 0), char, font=font, fill=1)
        
        font_offset = idx * FONT_BYTES
        for y in range(FONT_HEIGHT):
            row = 0
            for x in range(FONT_WIDTH):
                if img.getpixel((x, y)):
                    row |= (1 << (FONT_WIDTH - 1 - x))
            font_data[font_offset + y * 2] = (row >> 8) & 0xFF
            font_data[font_offset + y * 2 + 1] = row & 0xFF
    
    return font_data, unicode_to_gbk, valid_count

def write_files(font_data, unicode_to_gbk, valid_count, output_dir, file_prefix):
    os.makedirs(output_dir, exist_ok=True)
    
    unicode_map_file = os.path.join(output_dir, f"{file_prefix}_unicode_to_gbk.bin")
    font_data_file = os.path.join(output_dir, f"{file_prefix}_font_data.bin")
    header_file = os.path.join(output_dir, f"{file_prefix}_font_data.h")
    
    with open(unicode_map_file, 'wb') as f:
        f.write(unicode_to_gbk)
    
    with open(font_data_file, 'wb') as f:
        f.write(font_data)
    
    with open(header_file, 'w', encoding='utf-8') as f:
        f.write(f"#ifndef {file_prefix.upper()}_FONT_DATA_H\n")
        f.write(f"#define {file_prefix.upper()}_FONT_DATA_H\n")
        f.write("\n")
        f.write("#include <stdint.h>\n")
        f.write("\n")
        f.write("#define FONT_WIDTH 16\n")
        f.write("#define FONT_HEIGHT 16\n")
        f.write(f"#define {file_prefix.upper()}_FONT_TOTAL_COUNT {UNICODE_CJK_COUNT}\n")
        f.write(f"#define {file_prefix.upper()}_FONT_BYTES {FONT_BYTES}\n")
        f.write("\n")
        f.write(f"extern const uint8_t _binary_{file_prefix}_unicode_to_gbk_bin_start[];\n")
        f.write(f"extern const uint8_t _binary_{file_prefix}_unicode_to_gbk_bin_end[];\n")
        f.write(f"extern const uint8_t _binary_{file_prefix}_font_data_bin_start[];\n")
        f.write(f"extern const uint8_t _binary_{file_prefix}_font_data_bin_end[];\n")
        f.write("\n")
        f.write(f"#define {file_prefix}_unicode_to_gbk ((const uint16_t *)_binary_{file_prefix}_unicode_to_gbk_bin_start)\n")
        f.write(f"#define {file_prefix}_font_data _binary_{file_prefix}_font_data_bin_start\n")
        f.write("\n")
        f.write(f"#endif\n")
    
    return unicode_map_file, font_data_file, header_file

def main():
    parser = argparse.ArgumentParser(
        description='GBK字模生成工具 - 从TTF字体文件生成GBK全量汉字字模数据',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
使用示例:
    python generate_gbk_font.py "C:/Windows/Fonts/simsun.ttc"
        使用默认参数生成，输出到字体文件同目录，前缀为字体文件名
    
    python generate_gbk_font.py "C:/Windows/Fonts/simsun.ttc" -o "../main" -p "gbk" -s 16
        指定输出目录为../main，文件前缀为gbk，字体大小为16
    
    python generate_gbk_font.py "yuesong.ttf" -o "D:/esp/components/chenyong/font_service" -p "yuesong"
        生成月松字体字模到font_service组件目录

生成的文件:
    {prefix}_unicode_to_gbk.bin   - Unicode到GBK编码映射表（约40KB）
    {prefix}_font_data.bin        - GBK全量汉字字模数据（约653KB）
    {prefix}_font_data.h          - C语言头文件，定义数据访问宏

字体规格:
    点阵大小: 16x16
    每个字模: 32字节
    支持汉字: GBK编码范围内的全部汉字（约20902个）
    
注意事项:
    1. 需要安装Pillow库: pip install pillow
    2. TTF字体文件必须支持中文（GBK编码）
    3. 生成的字模数据可嵌入ESP32 Flash或写入外部W25Q128
        '''
    )
    parser.add_argument('font_path', help='TTF字体文件的路径（必填）')
    parser.add_argument('-o', '--output', dest='output_dir', default=None, 
                        help='输出目录，默认为字体文件所在目录')
    parser.add_argument('-p', '--prefix', dest='file_prefix', default=None,
                        help='输出文件前缀，默认为字体文件名（不含扩展名）')
    parser.add_argument('-s', '--size', dest='font_size', type=int, default=16,
                        help='渲染字体大小，默认为16')
    
    args = parser.parse_args()
    
    font_path = args.font_path
    font_size = args.font_size
    
    if not os.path.exists(font_path):
        print(f"Error: Font file not found: {font_path}")
        sys.exit(1)
    
    font_filename = os.path.basename(font_path)
    file_prefix = args.file_prefix if args.file_prefix else os.path.splitext(font_filename)[0]
    
    if args.output_dir:
        output_dir = args.output_dir
    else:
        output_dir = os.path.dirname(font_path)
        if not output_dir:
            output_dir = '.'
    
    print(f"Font file: {font_path}")
    print(f"Font size: {font_size}")
    print(f"Output directory: {output_dir}")
    print(f"File prefix: {file_prefix}")
    print()
    
    print("Generating font data...")
    font_data, unicode_to_gbk, valid_count = generate_font_data(font_path, font_size)
    
    print("Writing files...")
    unicode_map_file, font_data_file, header_file = write_files(font_data, unicode_to_gbk, valid_count, output_dir, file_prefix)
    
    print()
    print("Done!")
    print(f"Valid characters: {valid_count}")
    print(f"Total entries: {UNICODE_CJK_COUNT}")
    print(f"Font data size: {len(font_data)} bytes ({len(font_data) // 1024} KB)")
    print(f"Mapping table size: {len(unicode_to_gbk)} bytes ({len(unicode_to_gbk) // 1024} KB)")
    print()
    print("Generated files:")
    print(f"  - {unicode_map_file}")
    print(f"  - {font_data_file}")
    print(f"  - {header_file}")

if __name__ == '__main__':
    main()