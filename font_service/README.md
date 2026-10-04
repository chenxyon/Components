# Font Service 组件开发文档

## 一、组件概述

Font Service 是一个基于 ESP-IDF 的字体服务组件，提供统一的字体抽象接口，支持多种字体实现方式的条件编译和动态切换。

### 特性

- **统一接口**：所有字体实现遵循相同的 `FontService` 接口，便于上层应用调用
- **条件编译**：通过 `menuconfig` 配置，只编译选中的字体模块，节省 Flash 空间
- **多字体支持**：支持内置字体、GBK 全量汉字、外部 W25Q128 字体等多种实现
- **工厂模式**：通过 `font_service_create()` 函数统一创建字体实例

### 字体规格

- 点阵大小：16x16
- 每个字模：32 字节（16行 × 2字节/行）
- 编码范围：Unicode 0x4E00 - 0x9FA5（GBK 汉字范围）

### 编译策略

- **FONT_TYPE_BUILTIN**：始终编译，作为回退字体
- **FONT_TYPE_GBK**：始终编译，提供全量汉字支持
- **FONT_TYPE_YUESONG**：始终编译，提供月松字体支持
- **FONT_TYPE_W25Q**：始终编译，预留外部字体接口

> 注意：GBK 和 Yuesong 字体各占约 653KB Flash，同时启用需要 4MB Flash 和 3MB App 分区。

---

## 二、目录结构

```
font_service/
├── include/                  # 头文件目录
│   ├── font_service.h        # 字体服务抽象接口
│   ├── font_builtin.h        # 内置字体头文件
│   ├── font_gbk.h            # GBK 字体头文件
│   ├── font_yuesong.h        # Yuesong 字体头文件
│   ├── font_w25q.h           # W25Q128 外部字体头文件
│   ├── font_chinese_longyin.h # 内置字模数据
│   └── font_gbk_data.h       # GBK 字模数据头文件
├── src/                      # 源文件目录
│   ├── font_service.c        # 字体服务工厂函数
│   ├── font_builtin.c        # 内置字体实现
│   ├── font_gbk.c            # GBK 字体实现
│   ├── font_yuesong.c        # Yuesong 字体实现
│   ├── font_w25q.c           # W25Q128 字体实现（预留）
│   ├── font_chinese_longyin.c # 内置字模数据
│   └── font_display.c        # 汉字显示函数
├── tools/                    # 工具脚本目录
│   └── generate_gbk_font.py  # TTF字体转字模工具
├── CMakeLists.txt            # CMake 构建配置
├── Kconfig                   # menuconfig 配置选项
├── idf_component.yml         # 组件描述文件
├── README.md                 # 开发文档（本文件）
├── yuesong_unicode_to_gbk.bin # Yuesong Unicode映射表
├── yuesong_font_data.bin     # Yuesong 字模数据
└── yuesong_font_data.h       # Yuesong 头文件
```

---

## 三、接口说明

### 3.1 字体类型枚举

字体类型枚举定义了所有支持的字体类型，每个类型对应一个独立的字体实现模块。

```c
/**
 * @brief 字体类型枚举
 * 
 * 使用示例:
 * @code
 * FontService *font = font_service_create(FONT_TYPE_GBK);
 * if (!font) {
 *     font = font_service_create(FONT_TYPE_BUILTIN);
 * }
 * @endcode
 */
typedef enum {
    FONT_TYPE_BUILTIN,     /**< 内置测试字体（17个常用字符），始终可用 */
    FONT_TYPE_GBK,         /**< GBK全量汉字字体（约20902个字符），需menuconfig启用 */
    FONT_TYPE_YUESONG,     /**< Yuesong字体（约20902个字符），需menuconfig启用 */
    FONT_TYPE_W25Q,        /**< W25Q128外部字体（预留接口），需menuconfig启用 */
    FONT_TYPE_MAX          /**< 字体类型数量，用于数组边界检查 */
} font_type_t;
```

**枚举值说明：**

| 枚举值 | 说明 | 启用条件 | 占用空间 |
|--------|------|----------|----------|
| `FONT_TYPE_BUILTIN` | 内置测试字体，包含17个常用字符 | 始终可用 | ~340字节 |
| `FONT_TYPE_GBK` | GBK全量汉字字体，支持Unicode 0x4E00-0x9FA5 | `CONFIG_FONT_SERVICE_ENABLE_GBK` | ~653KB |
| `FONT_TYPE_YUESONG` | Yuesong字体，使用月松字体渲染 | `CONFIG_FONT_SERVICE_ENABLE_YUESONG` | ~653KB |
| `FONT_TYPE_W25Q` | W25Q128外部字体（预留接口） | `CONFIG_FONT_SERVICE_ENABLE_W25Q` | 仅驱动代码 |
| `FONT_TYPE_MAX` | 字体类型数量，用于数组边界检查 | - | - |

**添加新字体类型步骤：**

1. 在 `font_type_t` 枚举中添加新类型（在 `FONT_TYPE_MAX` 之前）
2. 在 `font_service.c` 的 `font_service_create()` 函数中添加对应分支
3. 在 `CMakeLists.txt` 中添加条件编译规则
4. 在 `Kconfig` 中添加配置选项

---

### 3.2 FontService 结构体

`FontService` 是统一的字体服务抽象接口，所有字体实现都遵循此接口。

```c
/**
 * @brief 字体服务接口结构体
 * 
 * 统一的字体服务抽象接口，所有字体实现都遵循此接口。
 * 
 * 使用示例:
 * @code
 * FontService *font = font_service_create(FONT_TYPE_GBK);
 * uint8_t font_buf[FONT_BYTES];
 * 
 * // 获取汉字"中"的字模（Unicode: 0x4E2D）
 * if (font->get_font(font, 0x4E2D, font_buf) == 0) {
 *     // 成功获取字模，font_buf中存储16x16点阵数据
 *     printf("Font size: %d bytes\n", font->get_font_size(font));
 *     printf("Max chars: %d\n", font->get_max_chars(font));
 * }
 * @endcode
 */
struct FontService {
    font_type_t type;                              /**< 字体类型 */
    const char *name;                              /**< 字体名称（用于日志输出） */
    int (*get_font)(FontService *self, uint32_t unicode, uint8_t *buf);   /**< 获取字模 */
    int (*get_font_size)(FontService *self);       /**< 获取字模大小（字节） */
    int (*get_max_chars)(FontService *self);       /**< 获取最大字符数 */
};
```

**字段说明：**

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `font_type_t` | 字体类型枚举值 |
| `name` | `const char *` | 字体名称，用于日志输出 |
| `get_font` | 函数指针 | 获取指定Unicode字符的字模 |
| `get_font_size` | 函数指针 | 获取单个字模的字节大小 |
| `get_max_chars` | 函数指针 | 获取字体支持的最大字符数 |

---

### 3.3 工厂函数

```c
/**
 * @brief 字体服务工厂函数
 * 
 * 根据指定类型创建字体服务实例。如果该类型的字体未在menuconfig中启用，
 * 则返回NULL。建议在创建后检查返回值，并提供回退方案。
 * 
 * 使用示例:
 * @code
 * // 方式1：创建单一字体
 * FontService *font = font_service_create(FONT_TYPE_GBK);
 * if (!font) {
 *     ESP_LOGW(TAG, "GBK font not available, falling back to builtin");
 *     font = font_service_create(FONT_TYPE_BUILTIN);
 * }
 * 
 * // 方式2：创建多个字体供切换
 * FontService *font_gbk = font_service_create(FONT_TYPE_GBK);
 * FontService *font_yuesong = font_service_create(FONT_TYPE_YUESONG);
 * 
 * // 使用当前字体（带多层回退）
 * FontService *current_font = font_gbk;
 * if (!current_font) current_font = font_yuesong;
 * if (!current_font) current_font = font_service_create(FONT_TYPE_BUILTIN);
 * @endcode
 * 
 * @param type 字体类型
 * @return 字体服务实例（成功）或NULL（失败，字体未启用）
 */
FontService *font_service_create(font_type_t type);
```

**工厂函数工作原理：**

工厂函数根据 `menuconfig` 配置的宏来决定是否编译和创建特定类型的字体：

```c
FontService *font_service_create(font_type_t type) {
    FontService *service = NULL;
    
    switch (type) {
#ifdef CONFIG_FONT_SERVICE_ENABLE_BUILTIN
        case FONT_TYPE_BUILTIN:
            service = font_builtin_create();
            break;
#endif
#ifdef CONFIG_FONT_SERVICE_ENABLE_GBK
        case FONT_TYPE_GBK:
            service = font_gbk_create();
            break;
#endif
#ifdef CONFIG_FONT_SERVICE_ENABLE_YUESONG
        case FONT_TYPE_YUESONG:
            service = font_yuesong_create();
            break;
#endif
        // ... 其他字体类型
    }
    
    return service;
}
```

**添加新字体到工厂函数步骤：**

1. 在 `font_service.c` 中添加条件编译分支
2. 使用 `extern` 声明新字体的创建函数
3. 调用创建函数并返回实例

---

### 3.4 接口方法

| 方法 | 原型 | 说明 | 返回值 |
|------|------|------|--------|
| `get_font` | `int (*get_font)(FontService *self, uint32_t unicode, uint8_t *buf)` | 获取指定Unicode字符的字模 | 0=成功, -1=失败 |
| `get_font_size` | `int (*get_font_size)(FontService *self)` | 获取单个字模的字节数 | 字模大小（通常为32） |
| `get_max_chars` | `int (*get_max_chars)(FontService *self)` | 获取字体支持的最大字符数 | 字符数量 |

**方法使用示例：**

```c
#include "font_service.h"

// 初始化字体服务
FontService *font = font_service_create(FONT_TYPE_GBK);
if (!font) {
    font = font_service_create(FONT_TYPE_BUILTIN);
}

// 获取字模信息
int font_size = font->get_font_size(font);      // 返回32（16x16点阵）
int max_chars = font->get_max_chars(font);      // 返回20902

// 获取汉字字模
uint8_t font_buf[FONT_BYTES];
int ret = font->get_font(font, 0x4E2D, font_buf);  // 获取"中"字的字模
if (ret == 0) {
    // 成功获取，font_buf包含32字节的点阵数据
} else {
    // 获取失败，字符不在字体范围内
}
```

---

## 四、配置选项

通过 `idf.py menuconfig` 进入 `Font Service Configuration` 菜单配置：

### 4.1 默认字体选择

```
Default Font
    ├── Built-in Font (17 chars)      # 内置测试字体
    ├── GBK Full Font (20K chars)     # GBK 全量汉字（默认）
    ├── Yuesong Font (20K chars)      # Yuesong 字体
    └── W25Q128 External Font         # W25Q128 外部字体
```

### 4.2 字体启用开关

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `CONFIG_FONT_SERVICE_ENABLE_BUILTIN` | 启用内置字体 | 是 |
| `CONFIG_FONT_SERVICE_ENABLE_GBK` | 启用 GBK 字体 | 默认字体为GBK时是 |
| `CONFIG_FONT_SERVICE_ENABLE_YUESONG` | 启用 Yuesong 字体 | 默认字体为Yuesong时是 |
| `CONFIG_FONT_SERVICE_ENABLE_W25Q` | 启用 W25Q128 字体 | 否（预留） |

### 4.3 编译优化

| 字体 | 启用配置 | 占用 Flash |
|------|----------|------------|
| Built-in | `CONFIG_FONT_SERVICE_ENABLE_BUILTIN` | ~340 字节 |
| GBK | `CONFIG_FONT_SERVICE_ENABLE_GBK` | ~653 KB |
| Yuesong | `CONFIG_FONT_SERVICE_ENABLE_YUESONG` | ~653 KB |
| W25Q | `CONFIG_FONT_SERVICE_ENABLE_W25Q` | 仅驱动代码 |

---

## 五、使用示例

### 5.1 完整初始化示例

推荐的字体服务初始化方式，包含多层回退机制：

```c
#include "font_service.h"
#include "esp_log.h"

static const char *TAG = "FONT_DEMO";
static FontService *font_service = NULL;

void font_service_init(void) {
    // 优先尝试创建 GBK 字体
    font_service = font_service_create(FONT_TYPE_GBK);
    
    // 如果 GBK 未启用，尝试 Yuesong 字体
    if (!font_service) {
        ESP_LOGW(TAG, "GBK font not available, trying Yuesong");
        font_service = font_service_create(FONT_TYPE_YUESONG);
    }
    
    // 最后回退到内置字体（始终可用）
    if (!font_service) {
        ESP_LOGW(TAG, "Yuesong font not available, falling back to builtin");
        font_service = font_service_create(FONT_TYPE_BUILTIN);
    }
    
    // 打印字体信息
    if (font_service) {
        ESP_LOGI(TAG, "Font service initialized: %s", font_service->name);
        ESP_LOGI(TAG, "Font size: %d bytes, Max chars: %d", 
                 font_service->get_font_size(font_service),
                 font_service->get_max_chars(font_service));
    }
}

void app_main() {
    // 初始化字体服务
    font_service_init();
    
    // 使用字体服务
    if (font_service) {
        uint8_t font_buf[FONT_BYTES];
        
        // 获取汉字"中"的字模（Unicode: 0x4E2D）
        if (font_service->get_font(font_service, 0x4E2D, font_buf) == 0) {
            ESP_LOGI(TAG, "Successfully got font for '中'");
            // 显示字模...
        } else {
            ESP_LOGW(TAG, "Failed to get font for '中'");
        }
    }
}
```

### 5.2 多字体切换示例

创建多个字体实例，根据需要动态切换：

```c
#include "font_service.h"

// 创建多个字体实例
FontService *font_gbk = font_service_create(FONT_TYPE_GBK);
FontService *font_yuesong = font_service_create(FONT_TYPE_YUESONG);

// 当前使用的字体
FontService *current_font = NULL;

// 切换字体函数
void switch_font(font_type_t type) {
    switch (type) {
        case FONT_TYPE_GBK:
            current_font = font_gbk;
            break;
        case FONT_TYPE_YUESONG:
            current_font = font_yuesong;
            break;
        case FONT_TYPE_BUILTIN:
            current_font = font_service_create(FONT_TYPE_BUILTIN);
            break;
        default:
            current_font = font_service_create(FONT_TYPE_BUILTIN);
            break;
    }
}

// 使用示例
void display_text_with_font(const char *text, font_type_t font_type) {
    // 切换到指定字体
    switch_font(font_type);
    
    if (!current_font) {
        // 字体不可用，回退到内置字体
        current_font = font_service_create(FONT_TYPE_BUILTIN);
    }
    
    // 显示文本...
}
```

### 5.3 字模显示示例

完整的汉字显示函数实现：

```c
#include "font_service.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define FONT_WIDTH 16
#define FONT_HEIGHT 16

static const char *TAG = "FONT_DISPLAY";

void lcd_show_chinese_char(uint16_t x, uint16_t y, uint32_t unicode, uint16_t color, FontService *font) {
    if (!font) return;
    
    uint8_t font_buf[FONT_BYTES];
    if (font->get_font(font, unicode, font_buf) != 0) {
        ESP_LOGW(TAG, "Font not found for unicode: 0x%04X", unicode);
        return;
    }
    
    // 设置显示窗口
    lcd_set_window(x, y, x + FONT_WIDTH - 1, y + FONT_HEIGHT - 1);
    
    // 逐行绘制字模
    for (uint8_t row = 0; row < FONT_HEIGHT; row++) {
        uint16_t pixel_data = (font_buf[row * 2] << 8) | font_buf[row * 2 + 1];
        for (uint8_t col = 0; col < FONT_WIDTH; col++) {
            // 从高位到低位逐位判断
            uint16_t pixel_color = (pixel_data & 0x8000) ? color : 0x0000;
            lcd_write_data16(pixel_color);
            pixel_data <<= 1;
        }
    }
}

// 使用示例
void display_chinese_text(uint16_t x, uint16_t y, const uint32_t *unicode_array, size_t count, uint16_t color) {
    FontService *font = font_service_create(FONT_TYPE_GBK);
    if (!font) {
        font = font_service_create(FONT_TYPE_BUILTIN);
    }
    
    uint16_t current_x = x;
    for (size_t i = 0; i < count; i++) {
        lcd_show_chinese_char(current_x, y, unicode_array[i], color, font);
        current_x += FONT_WIDTH;
    }
}
```

---

## 六、新增字体库操作步骤

### 步骤1：准备 TTF 字体文件

准备好要转换的 TTF 字体文件，确保该字体支持中文（GBK 编码）。

### 步骤2：生成字模数据

使用 `generate_gbk_font.py` 工具生成字模数据：

**方式1：使用组件自带工具（推荐）**

```bash
cd D:/esp/components/chenyong/font_service
python tools/generate_gbk_font.py "path/to/your/font.ttf" \
    -o . \
    -p "your_font_name" \
    -s 16
```

**方式2：使用全局工具**

```bash
python D:/esp/tools/generate_gbk_font.py "path/to/your/font.ttf" \
    -o "D:/esp/components/chenyong/font_service" \
    -p "your_font_name" \
    -s 16
```

**参数说明：**

| 参数 | 说明 |
|------|------|
| `font_path` | TTF 字体文件路径（必填） |
| `-o, --output` | 输出目录，默认为字体文件所在目录 |
| `-p, --prefix` | 输出文件前缀（如：your_font_name） |
| `-s, --size` | 渲染字体大小，默认为16 |

**生成的文件：**

| 文件 | 说明 |
|------|------|
| `{prefix}_unicode_to_gbk.bin` | Unicode 到 GBK 编码映射表（约40KB） |
| `{prefix}_font_data.bin` | GBK 全量汉字字模数据（约653KB） |
| `{prefix}_font_data.h` | C 语言头文件，定义数据访问宏 |

### 步骤3：创建字体头文件

在 `include/` 目录下创建 `font_{font_name}.h`：

```c
#ifndef FONT_{FONT_NAME}_H
#define FONT_{FONT_NAME}_H

#include "font_service.h"

FontService *font_{font_name}_create(void);

#endif
```

**示例**（font_yuesong.h）：

```c
#ifndef FONT_YUESONG_H
#define FONT_YUESONG_H

#include "font_service.h"

FontService *font_yuesong_create(void);

#endif
```

### 步骤4：创建字体实现文件

在 `src/` 目录下创建 `font_{font_name}.c`：

```c
#include "font_{font_name}.h"
#include "{font_name}_font_data.h"
#include "font_service.h"
#include <string.h>

typedef struct {
    FontService base;
} Font{FontName};

static int font_{font_name}_get_font(FontService *self, uint32_t unicode, uint8_t *buf) {
    if (unicode < 0x4E00 || unicode > 0x9FA5) {
        return -1;
    }
    
    uint32_t idx = unicode - 0x4E00;
    uint16_t gbk_code = {font_name}_unicode_to_gbk[idx];
    
    if (gbk_code == 0) {
        return -1;
    }
    
    uint32_t offset = idx * {FONT_NAME}_FONT_BYTES;
    memcpy(buf, &{font_name}_font_data[offset], {FONT_NAME}_FONT_BYTES);
    
    return 0;
}

static int font_{font_name}_get_font_size(FontService *self) {
    return {FONT_NAME}_FONT_BYTES;
}

static int font_{font_name}_get_max_chars(FontService *self) {
    return {FONT_NAME}_FONT_TOTAL_COUNT;
}

FontService *font_{font_name}_create(void) {
    static Font{FontName} instance;
    instance.base.type = FONT_TYPE_{FONT_NAME};
    instance.base.name = "{FontName}";
    instance.base.get_font = font_{font_name}_get_font;
    instance.base.get_font_size = font_{font_name}_get_font_size;
    instance.base.get_max_chars = font_{font_name}_get_max_chars;
    return &instance.base;
}
```

**示例**（font_yuesong.c）：

```c
#include "font_yuesong.h"
#include "yuesong_font_data.h"
#include "font_service.h"
#include <string.h>

typedef struct {
    FontService base;
} FontYuesong;

static int font_yuesong_get_font(FontService *self, uint32_t unicode, uint8_t *buf) {
    if (unicode < 0x4E00 || unicode > 0x9FA5) {
        return -1;
    }
    
    uint32_t idx = unicode - 0x4E00;
    uint16_t gbk_code = yuesong_unicode_to_gbk[idx];
    
    if (gbk_code == 0) {
        return -1;
    }
    
    uint32_t offset = idx * YUESONG_FONT_BYTES;
    memcpy(buf, &yuesong_font_data[offset], YUESONG_FONT_BYTES);
    
    return 0;
}

static int font_yuesong_get_font_size(FontService *self) {
    return YUESONG_FONT_BYTES;
}

static int font_yuesong_get_max_chars(FontService *self) {
    return YUESONG_FONT_TOTAL_COUNT;
}

FontService *font_yuesong_create(void) {
    static FontYuesong instance;
    instance.base.type = FONT_TYPE_YUESONG;
    instance.base.name = "Yuesong";
    instance.base.get_font = font_yuesong_get_font;
    instance.base.get_font_size = font_yuesong_get_font_size;
    instance.base.get_max_chars = font_yuesong_get_max_chars;
    return &instance.base;
}
```

### 步骤5：更新字体类型枚举

在 `include/font_service.h` 的 `font_type_t` 枚举中添加新字体类型：

```c
typedef enum {
    FONT_TYPE_BUILTIN,
    FONT_TYPE_GBK,
    FONT_TYPE_YUESONG,    // 添加新字体类型
    FONT_TYPE_YOUR_FONT,  // 示例：添加你的字体
    FONT_TYPE_W25Q,
    FONT_TYPE_MAX
} font_type_t;
```

### 步骤6：更新工厂函数

在 `src/font_service.c` 的 `font_service_create()` 函数中添加新字体的创建分支：

```c
#ifdef CONFIG_FONT_SERVICE_ENABLE_YOUR_FONT
        case FONT_TYPE_YOUR_FONT:
            extern FontService *font_your_font_create(void);
            service = font_your_font_create();
            break;
#endif
```

### 步骤7：更新 CMakeLists.txt

在 `CMakeLists.txt` 中添加条件编译规则：

```cmake
if(CONFIG_FONT_SERVICE_ENABLE_YOUR_FONT)
    target_sources(${COMPONENT_LIB} PRIVATE "src/font_your_font.c")
    target_include_directories(${COMPONENT_LIB} PRIVATE ".")
    target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/your_font_unicode_to_gbk.bin" BINARY)
    target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/your_font_font_data.bin" BINARY)
endif()
```

### 步骤8：更新 Kconfig

在 `Kconfig` 中添加配置选项：

1. 在 `choice FONT_SERVICE_DEFAULT_FONT` 中添加：

```kconfig
config FONT_SERVICE_DEFAULT_FONT_YOUR_FONT
    bool "Your Font (20K chars)"
    help
        Use Your Font with all Chinese characters.
```

2. 在 `choice` 结束后添加启用开关：

```kconfig
config FONT_SERVICE_ENABLE_YOUR_FONT
    bool "Enable Your Font"
    default y if FONT_SERVICE_DEFAULT_FONT_YOUR_FONT
    default n
    depends on FONT_SERVICE_ENABLE
    help
        Enable Your Font support (about 653KB flash).
```

### 步骤9：验证编译

```bash
cd your_project
idf.py menuconfig
# 在 Font Service Configuration 中启用新字体
idf.py build
```

---

## 七、注意事项

### 7.1 Flash 空间限制

- ESP32-S3 N16R8 拥有 16MB Flash，足够容纳多个字体
- 单个 GBK 全量汉字字体约占 653KB Flash
- 建议根据项目需求，只启用需要的字体

### 7.2 字体兼容性

- TTF 字体文件必须支持中文（GBK 编码）
- 某些字体可能缺少部分汉字，生成时会跳过这些字符
- 建议使用完整的中文字体（如：宋体、黑体、楷体等）

### 7.3 W25Q128 外部字体

#### 7.3.1 概述

W25Q128 是一款 128 Mbit（16 MB）的 SPI Flash 芯片，可以作为外部字体存储介质，不占用 ESP32 内部 Flash 空间。

**特点：**
- 大容量：16 MB，可存储约 23 个 GBK 全量汉字字体
- 灵活：支持动态更新字体，无需重新编译固件
- 节省空间：仅占用驱动代码（约几 KB），不占用内部 Flash

#### 7.3.2 存储布局设计（推荐方案）

W25Q128 建议采用分区存储方式，支持多个字体共存：

```
W25Q128 存储布局（16 MB）:
┌──────────────────────────────────────────────────────────────┐
│ 0x000000 - 0x000100  │ 文件头（字体数量、版本信息等）          │
├──────────────────────────────────────────────────────────────┤
│ 0x000100 - 0x00A500  │ 字体1: Unicode→GBK映射表（约40KB）      │
├──────────────────────────────────────────────────────────────┤
│ 0x00A500 - 0x44A500  │ 字体1: GBK字模数据（约653KB）          │
├──────────────────────────────────────────────────────────────┤
│ 0x44A500 - 0x44A600  │ 字体2: Unicode→GBK映射表（约40KB）      │
├──────────────────────────────────────────────────────────────┤
│ 0x44A600 - 0x88A600  │ 字体2: 字模数据（约653KB）            │
├──────────────────────────────────────────────────────────────┤
│ 0x88A600 - 0x88A700  │ 字体3: Unicode→GBK映射表（约40KB）      │
├──────────────────────────────────────────────────────────────┤
│ 0x88A700 - 0xCCC700  │ 字体3: 字模数据（约653KB）            │
├──────────────────────────────────────────────────────────────┤
│ 0xCCC700 - 0xFFFFFF  │ 预留空间（约13 MB，可扩展更多字体）      │
└──────────────────────────────────────────────────────────────┘
```

#### 7.3.3 文件头格式

文件头位于偏移 0x000000，大小为 256 字节：

| 偏移 | 大小 | 说明 |
|------|------|------|
| 0x00 | 4 | 魔数："FONT"（0x464F4E54） |
| 0x04 | 2 | 版本号（主版本.次版本） |
| 0x06 | 2 | 字体数量 |
| 0x08 | 256 - 8 | 字体信息表（每字体占用固定字节，存储名称、偏移、大小等） |

#### 7.3.4 单字体存储结构

每个字体占用约 693 KB：

| 区域 | 大小 | 说明 |
|------|------|------|
| Unicode→GBK映射表 | ~40 KB | 20902 × 2 字节 |
| GBK字模数据 | ~653 KB | 20902 × 32 字节 |
| **合计** | **~693 KB** | |

#### 7.3.5 字体索引表

| 字体索引 | 映射表偏移 | 字模偏移 | 总大小 | 用途 |
|----------|-----------|---------|--------|------|
| 0 | 0x000100 | 0x00A500 | ~693 KB | 主字体（如：宋体） |
| 1 | 0x44A500 | 0x44A600 | ~693 KB | 备用字体1（如：Yuesong） |
| 2 | 0x88A600 | 0x88A700 | ~693 KB | 备用字体2（如：楷体） |
| ... | ... | ... | ... | ... |
| N | 动态计算 | 动态计算 | ~693 KB | 扩展字体 |

#### 7.3.6 实现计划

当前 W25Q128 字体功能为预留接口，后续开发步骤：

1. **实现 W25Q128 SPI 驱动**：添加 SPI Flash 读写函数
2. **实现字库烧录工具**：通过 ESP32 将字模数据写入 W25Q128
3. **实现多字体管理**：支持根据索引切换不同字体
4. **完善 font_w25q.c**：实现 `font_w25q_get_font()` 函数的实际读取逻辑

#### 7.3.7 预留接口设计

[font_w25q.c](src/font_w25q.c) 中的结构体已预留偏移量字段：

```c
typedef struct {
    FontService base;
    uint32_t font_offset;   // 字模在W25Q128中的起始偏移
    uint32_t font_count;    // 字符数量
} FontW25q;
```

后续实现时，可通过不同的 `font_offset` 来切换不同字体：

```c
// 创建主字体服务（从偏移0x00A500开始）
FontService *font_primary = font_w25q_create_with_offset(0x00A500);

// 创建备用字体服务（从偏移0x44A600开始）
FontService *font_secondary = font_w25q_create_with_offset(0x44A600);
```

---

## 八、版本管理

本组件已集成 `version_manager`，支持独立的版本号管理。

### 使用方式

```c
#include "font_service_version.h"

printf("Font Service Version: %s\n", FONT_SERVICE_VERSION_STRING);
printf("Major: %d, Minor: %d, Patch: %d\n",
       FONT_SERVICE_VERSION_MAJOR,
       FONT_SERVICE_VERSION_MINOR,
       FONT_SERVICE_VERSION_PATCH);
```

### 生成的宏定义

| 宏名 | 说明 |
|------|------|
| `FONT_SERVICE_VERSION_MAJOR` | 主版本号 |
| `FONT_SERVICE_VERSION_MINOR` | 次版本号 |
| `FONT_SERVICE_VERSION_PATCH` | 补丁版本号 |
| `FONT_SERVICE_VERSION_STRING` | 版本字符串 |

### 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0.0 | 2026-07-11 | 初始版本，支持内置字体和 GBK 字体 |
| v1.1.0 | 2026-07-11 | 添加 Yuesong 字体支持，完善条件编译 |
| v1.2.0 | 2026-07-11 | 完善 W25Q128 多字体存储方案设计文档 |
| v1.3.0 | 2026-07-11 | 添加 tools 目录（generate_gbk_font.py），更新开发文档，完善接口说明和使用示例 |
| v1.4.0 | 2026-07-12 | 集成 version_manager，支持组件独立版本号 |
| v1.5.0 | 2026-07-12 | 新增 font_display.c/h 显示模块，提供 `font_display_char()` 和 `font_display_string()` API；修改编译策略，所有字体类型无条件编译；添加 LCD 操作回调接口（FontLcdOps）；更新全局规则添加组件修改审核规则 |

---

## 九、全局组件开发规则

### 组件存储位置

所有自开发组件统一存放在：

```
D:/esp/components/chenyong/
```

### 版本管理要求

1. **必须**在组件的 `CMakeLists.txt` 中集成 `version_manager`
2. **必须**调用 `register_component_version(NAME "组件名")` 注册版本
3. **必须**创建 `version.txt` 文件，初始版本号为 `1.0.000`
4. **必须**在组件的 `README.md` 中记录版本历史

### 文档更新要求

1. 修改代码时，**必须**同步更新组件的 `README.md`
2. 文档中**必须**包含：
   - 组件概述和特性
   - 接口说明和使用示例
   - 配置选项（如有）
   - 版本历史

### 组件命名规范

- 使用小写字母和下划线（如：`font_service`、`version_manager`）
- 避免使用中文和特殊字符

---

## 十、许可证

本组件基于 MIT 许可证开源。