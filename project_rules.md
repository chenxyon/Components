# ESP-IDF 组件开发全局规则

## 一、组件存储位置

所有自开发组件统一存放在：

```
D:/esp/components/chenyong/
```

### 目录结构

```
D:/esp/components/chenyong/
├── version_manager/           # 版本管理组件
├── font_service/              # 字体服务组件
├── tft_ili9341/               # TFT ILI9341 驱动组件
├── project_rules.md           # 全局开发规则（本文件）
└── ...                        # 其他组件
```

## 二、组件命名规范

### 组件名称

- 使用小写字母和下划线（如：`font_service`、`version_manager`）
- 避免使用中文和特殊字符
- 使用有意义的名称，体现组件功能

### 文件命名

- 头文件：`{component_name}.h`（如：`font_service.h`）
- 源文件：`{component_name}.c`（如：`font_service.c`）
- 版本头文件（自动生成）：`{component_name}_version.h`

## 三、版本管理规则

### 3.1 必须集成 version_manager

每个组件**必须**在 `CMakeLists.txt` 中集成 `version_manager`：

```cmake
idf_component_register(
    SRCS "src/my_component.c"
    INCLUDE_DIRS "include"
    REQUIRES "version_manager"
)

idf_component_get_property(version_manager_PATH version_manager COMPONENT_DIR)
include(${version_manager_PATH}/cmake/version_helper.cmake)

register_component_version(NAME "my_component")
```

### 3.2 必须创建 version.txt

每个组件**必须**创建 `version.txt` 文件，初始版本号为 `1.0.000`：

```bash
echo "1.0.000" > D:/esp/components/chenyong/my_component/version.txt
```

### 3.3 版本号格式

- 格式：`主版本.次版本.补丁版本`（例如：`1.0.001`）
- 补丁版本为3位数字，不足3位时前面补0

### 3.4 版本号递增规则

- **开发阶段**：每次编译自动递增补丁版本号
- **功能更新**：递增次版本号，补丁版本号归零
- **重大变更**：递增主版本号，次版本号和补丁版本号归零

### 3.5 版本号互不冲突机制

每个组件通过唯一的组件名称生成独立的宏定义前缀：

| 组件名称 | 宏前缀 | 头文件 |
|----------|--------|--------|
| version_manager | `VERSION_MANAGER_VERSION_*` | `version_manager_version.h` |
| font_service | `FONT_SERVICE_VERSION_*` | `font_service_version.h` |
| my_component | `MY_COMPONENT_VERSION_*` | `my_component_version.h` |

这些宏定义完全独立，不会相互冲突。

### 3.6 在代码中使用版本号

```c
#include "my_component_version.h"

void my_component_init(void) {
    printf("My Component Version: %s\n", MY_COMPONENT_VERSION_STRING);
    printf("Major: %d, Minor: %d, Patch: %d\n",
           MY_COMPONENT_VERSION_MAJOR,
           MY_COMPONENT_VERSION_MINOR,
           MY_COMPONENT_VERSION_PATCH);
}
```

## 四、文档管理规则

### 4.1 必须维护 README.md

每个组件**必须**包含 `README.md` 文件，记录组件的完整信息。

### 4.2 README.md 必须包含的内容

1. **组件概述**：组件的功能和特性
2. **目录结构**：组件的文件组织结构
3. **接口说明**：提供的 API 接口和使用示例
4. **配置选项**：menuconfig 配置项（如有）
5. **使用示例**：完整的代码使用示例
6. **版本历史**：记录版本变更

### 4.3 文档更新要求

- 修改代码时，**必须**同步更新 `README.md`
- 添加新功能时，**必须**更新版本历史
- 文档格式**必须**统一，使用 Markdown 格式

## 五、新增组件流程

### 步骤1：创建组件目录

```bash
mkdir -p D:/esp/components/chenyong/my_new_component/{include,src}
```

### 步骤2：创建 CMakeLists.txt

```cmake
idf_component_register(
    SRCS "src/my_new_component.c"
    INCLUDE_DIRS "include"
    REQUIRES "version_manager"
)

idf_component_get_property(version_manager_PATH version_manager COMPONENT_DIR)
include(${version_manager_PATH}/cmake/version_helper.cmake)

register_component_version(NAME "my_new_component")
```

### 步骤3：创建 version.txt

```bash
echo "1.0.000" > D:/esp/components/chenyong/my_new_component/version.txt
```

### 步骤4：创建组件代码

```c
// include/my_new_component.h
#ifndef MY_NEW_COMPONENT_H
#define MY_NEW_COMPONENT_H

void my_new_component_init(void);

#endif // MY_NEW_COMPONENT_H
```

```c
// src/my_new_component.c
#include "my_new_component.h"
#include "my_new_component_version.h"
#include "esp_log.h"

static const char *TAG = "MY_NEW_COMPONENT";

void my_new_component_init(void) {
    ESP_LOGI(TAG, "My New Component Version: %s", MY_NEW_COMPONENT_VERSION_STRING);
}
```

### 步骤5：创建 README.md

```markdown
# My New Component

## 简介

My New Component 是一个示例组件。

## 功能特性

- 示例功能1
- 示例功能2

## 使用方法

```c
#include "my_new_component.h"

my_new_component_init();
```

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0.0 | 2026-07-12 | 初始版本 |
```

### 步骤6：验证编译

```bash
cd your_project
idf.py build
```

## 六、代码风格规范

### 6.1 命名规范

- **变量名**：使用小写字母和下划线（如：`font_buf`）
- **函数名**：使用小写字母和下划线（如：`font_service_create`）
- **宏定义**：使用大写字母和下划线（如：`FONT_BYTES`）
- **结构体名**：使用 Pascal 命名法（如：`FontService`）

### 6.2 注释规范

- 使用 Doxygen 风格注释
- 为每个函数添加说明文档
- 为复杂的逻辑添加注释

### 6.3 文件头注释

每个源文件**必须**包含文件头注释：

```c
/**
 * @file my_new_component.c
 * @brief My New Component implementation
 *
 * This file contains the implementation of My New Component.
 */
```

## 七、Git 工作流

### 7.1 提交规范

- 使用英文描述提交内容
- 提交信息简洁明了，不超过50字符
- 使用以下前缀：
  - `feat:` 新功能
  - `fix:` 修复 Bug
  - `docs:` 更新文档
  - `refactor:` 重构代码
  - `chore:` 日常维护

### 7.2 分支管理

- `main`：主分支，存放稳定代码
- `develop`：开发分支，存放正在开发的代码
- `feature/*`：功能分支，开发新功能
- `bugfix/*`：修复分支，修复 Bug

### 7.3 版本标签

发布新版本时，**必须**创建 Git 标签：

```bash
git tag v1.0.000
git push origin v1.0.000
```

## 八、配置管理

### 8.1 Kconfig 配置

组件如有可配置选项，**必须**创建 `Kconfig` 文件：

```kconfig
menu "My New Component Configuration"

config MY_NEW_COMPONENT_ENABLE
    bool "Enable My New Component"
    default y
    help
        Enable My New Component support.

endmenu
```

### 8.2 idf_component.yml

每个组件**必须**创建 `idf_component.yml` 文件：

```yaml
version: "1.0.0"
description: "My New Component"
url: "https://github.com/your/repo"
dependencies:
  version_manager:
    path: ../version_manager
```

## 九、依赖管理

### 9.1 依赖声明

组件的依赖**必须**在 `CMakeLists.txt` 中声明：

```cmake
idf_component_register(
    SRCS "src/my_component.c"
    INCLUDE_DIRS "include"
    REQUIRES "version_manager"
    PRIV_REQUIRES "driver"
)
```

### 9.2 依赖顺序

- `REQUIRES`：公开依赖，头文件会暴露给使用者
- `PRIV_REQUIRES`：私有依赖，头文件不会暴露给使用者

## 十、组件修改审核规则

### 10.1 修改前检查

修改组件前，**必须**完成以下检查：

| 检查项 | 说明 |
|--------|------|
| 依赖确认 | 确认修改不会破坏其他依赖此组件的项目 |
| 文档同步 | 确认修改后会同步更新 README.md |
| 版本更新 | 确认版本号会相应递增 |
| 编译验证 | 修改后必须通过编译验证 |

### 10.2 修改记录要求

每次修改组件，**必须**在 `README.md` 的**版本历史**中记录：

| 字段 | 要求 |
|------|------|
| 版本号 | 按规则递增（主版本.次版本.补丁版本） |
| 日期 | 修改日期 |
| 说明 | 详细描述修改内容和影响 |

### 10.3 新增功能审核清单

新增功能时，**必须**完成以下审核项：

- [ ] 新增 API 接口是否清晰、易用
- [ ] 是否添加了完整的使用示例
- [ ] 是否更新了配置选项（Kconfig）
- [ ] 是否更新了构建配置（CMakeLists.txt）
- [ ] 是否通过编译验证
- [ ] 是否更新了文档

### 10.4 破坏性变更处理

如果修改涉及**破坏性变更**（如删除 API、修改接口签名）：

1. **必须**递增主版本号
2. **必须**在文档中明确标注变更内容
3. **必须**通知所有依赖此组件的项目维护者

## 十一、总结

| 规则类型 | 核心要求 |
|----------|----------|
| 组件存储 | 统一存放在 `D:/esp/components/chenyong/` |
| 版本管理 | 必须集成 `version_manager`，每个组件独立版本号 |
| 文档管理 | 必须维护 `README.md`，修改代码同步更新文档 |
| 代码风格 | 统一命名规范，使用 Doxygen 注释 |
| Git 工作流 | 规范提交信息，使用版本标签 |
| 修改审核 | 必须检查依赖、更新文档、验证编译 |

---

**最后更新**：2026-07-12
