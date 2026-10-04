# 本地 Git 仓库 + Path 引用 + 自动标签锁定 — 版本管理操作手册

## 一、方案概述

本方案采用 **本地 Git 仓库 + path 引用 + 自动标签锁定** 的混合模式，实现：

| 特性 | 说明 |
|------|------|
| 代码隔离 | 物理隔离旧版本代码，防止误修改 |
| 版本锁定 | 通过 Git 标签精确锁定组件版本 |
| 零网络依赖 | 纯本地/内网 Git，代码不出局域网 |
| 空间高效 | 所有项目共享同一本地仓库，无需多份复制 |
| 切换便捷 | 只需修改 `idf_component.yml` 中的 `version` 字段即可切换版本 |

---

## 二、环境准备

### 2.1 安装 Git

```bash
# Windows: 下载安装 https://git-scm.com/
# 验证安装
git --version
```

### 2.2 创建本地组件仓库根目录

```bash
mkdir -p D:\esp\components\chenyong\components
```

---

## 三、本地组件仓库搭建（一次性操作）

### 3.1 创建组件仓库

以 `display_driver` 组件为例：

```bash
# 1. 创建组件目录
mkdir -p D:\esp\components\chenyong\components\display_driver

# 2. 进入目录，初始化 Git（无需远程地址）
cd D:\esp\components\chenyong\components\display_driver
git init

# 3. 配置 Git 用户信息
git config user.email "your_email@company.com"
git config user.name "Your Name"
```

### 3.2 首次提交

```bash
# 1. 将组件代码复制到该目录
# 确保目录结构如下：
# display_driver/
# ├── include/
# ├── src/
# ├── CMakeLists.txt
# ├── idf_component.yml
# └── README.md

# 2. 添加所有文件
git add .

# 3. 提交
git commit -m "Initial commit - v1.0.0: Basic I2C OLED support"
```

### 3.3 打版本标签

```bash
# 给当前版本打标签
git tag -a v1.0.0 -m "v1.0.0: Basic I2C OLED support"

# 查看所有标签
git tag -l

# 查看标签详情
git show v1.0.0
```

### 3.4 同样的方式创建其他组件仓库

```bash
# 创建 system_scheduler 仓库
mkdir -p D:\esp\components\chenyong\components\system_scheduler
cd D:\esp\components\chenyong\components\system_scheduler
git init
git add .
git commit -m "Initial commit - v1.0.0: Event-driven scheduler"
git tag -a v1.0.0 -m "v1.0.0: Event-driven scheduler with watchdog"
```

---

## 四、项目引用组件

### 4.1 项目引用方式

#### 方式一：使用 EXTRA_COMPONENT_DIRS（推荐）

在项目根目录的 `CMakeLists.txt` 中添加：

```cmake
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS "D:/esp/components/chenyong/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(your_project)
```

**优点**：配置简单，无需管理多个 path 引用，适用于需要使用所有组件的场景。

#### 方式二：使用 `idf_component.yml`（精细版本控制）

在项目根目录创建或修改 `idf_component.yml`：

##### 项目A 使用 v1.0.0（旧版本）

```yaml
dependencies:
  display_driver:
    path: D:/esp/components/chenyong/components/display_driver
    version: "v1.0.0"
  
  system_scheduler:
    path: D:/esp/components/chenyong/components/system_scheduler
    version: "v1.0.0"
```

#### 项目B 使用 v2.0.0（新版本）

```yaml
dependencies:
  display_driver:
    path: D:/esp/components/chenyong/components/display_driver
    version: "v2.0.0"
  
  system_scheduler:
    path: D:/esp/components/chenyong/components/system_scheduler
    version: "v1.0.0"
```

### 4.2 编译验证

```bash
# 首次构建会自动 checkout 到指定标签
idf.py build

# 查看当前组件版本状态
cd D:\esp\components\chenyong\components\display_driver
git status  # 会显示 "HEAD detached at v1.0.0"
```

---

## 五、日常版本控制操作

### 5.1 开发新版本

```bash
# 1. 进入组件仓库
cd D:\esp\components\chenyong\components\display_driver

# 2. 创建开发分支
git checkout -b feature/v2.0.0

# 3. 修改代码...
#    - 添加新功能
#    - 修复 Bug
#    - 更新文档

# 4. 提交更改
git add .
git commit -m "feat: Add SPI TFT support"

# 5. 继续开发...
git add .
git commit -m "feat: Add auto text detection"
git add .
git commit -m "feat: Add rich text display"
```

### 5.2 发布新版本

```bash
# 1. 确保所有修改已提交
git status

# 2. 打版本标签
git tag -a v2.0.0 -m "v2.0.0: Add SPI TFT, auto text detection, rich text"

# 3. 切换回开发分支继续开发（可选）
git checkout feature/v2.0.0
```

### 5.3 更新现有项目到新版本

只需修改项目的 `idf_component.yml`：

```yaml
dependencies:
  display_driver:
    path: D:/esp/components/chenyong/components/display_driver
    version: "v2.0.0"  # 从 v1.0.0 改为 v2.0.0
```

然后重新构建：

```bash
idf.py build
```

### 5.4 回滚到旧版本

```yaml
dependencies:
  display_driver:
    path: D:/esp/components/chenyong/components/display_driver
    version: "v1.0.0"  # 改回旧版本
```

```bash
idf.py build
```

---

## 六、分支管理策略

### 6.1 分支命名规范

| 分支类型 | 命名格式 | 说明 |
|----------|----------|------|
| 主分支 | `main` | 稳定版本分支 |
| 开发分支 | `develop` | 日常开发分支 |
| 功能分支 | `feature/<功能名>` | 新功能开发 |
| Bug修复分支 | `fix/<bug描述>` | Bug 修复 |
| 发布分支 | `release/<版本号>` | 发布前测试 |

### 6.2 标准工作流程

```
main ──┐
       │ merge
       ▼
develop ──┐
          │ feature/v2.0.0
          ▼
feature/v2.0.0 ──┐
                 │ merge
                 ▼
release/v2.0.0 ──┐
                 │ tag v2.0.0
                 │ merge
                 ▼
              main
```

---

## 七、标签管理规范

### 7.1 标签命名规范

```
v<主版本号>.<次版本号>.<修订号>
```

- **主版本号**：不兼容的 API 变更
- **次版本号**：向后兼容的功能新增
- **修订号**：向后兼容的 Bug 修复

### 7.2 标签操作

```bash
# 查看所有标签
git tag -l

# 查看标签详情
git show v2.0.0

# 删除标签（谨慎操作）
git tag -d v2.0.0

# 查看标签历史
git log --oneline --decorate
```

---

## 八、安全防护措施

### 8.1 Git 分离头指针保护

当项目引用指定版本时，IDF 会自动执行 `git checkout v1.0.0`，此时仓库处于**分离头指针（detached HEAD）**状态。

**保护效果：**
- Git 会警告无法直接 commit
- 需要创建新分支才能保存修改
- 极大增加了"误保存"的操作成本

### 8.2 系统级只读锁（推荐）

为防止手误修改旧版本代码，给本地仓库文件夹添加只读权限：

**Windows：**
1. 右键 `D:\esp\components\chenyong\components\display_driver` → 属性
2. 勾选"只读" → 确定
3. 在弹出的确认对话框中选择"应用到文件夹、子文件夹和文件"

**发布新版本时临时解除：**
1. 取消"只读"勾选
2. 打完新标签后重新勾选

---

## 九、团队协作（内网共享方案）

### 9.1 共享本地仓库

将 `D:\esp\components\chenyong\components` 文件夹放在公司局域网共享盘：

```
//nas-server/shared/components/
├── display_driver/
├── system_scheduler/
└── fonts/
```

### 9.2 团队成员配置

每个成员在项目的 `idf_component.yml` 中指向网络路径：

```yaml
dependencies:
  display_driver:
    path: //nas-server/shared/components/display_driver
    version: "v2.0.0"
```

### 9.3 协作流程

```
1. 管理员在共享仓库开发新功能并打标签
2. 团队成员更新项目的 idf_component.yml 版本号
3. 执行 idf.py build 自动拉取新版本
4. 如需修改，管理员在开发分支修改后重新打标签
```

---

## 十、常见问题

### 10.1 Q: 切换版本后编译失败？

**A:** 检查是否有编译缓存问题，执行清理后重新编译：

```bash
idf.py fullclean
idf.py build
```

### 10.2 Q: 如何查看当前使用的组件版本？

**A:** 

```bash
cd D:\esp\components\chenyong\components\display_driver
git describe --tags
```

### 10.3 Q: 如何在开发分支和标签之间切换？

**A:**

```bash
# 切换到开发分支
git checkout develop

# 切换到标签（分离头指针状态）
git checkout v1.0.0

# 创建新分支基于标签
git checkout -b fix/v1.0.1 v1.0.0
```

### 10.4 Q: 只读权限导致无法 checkout？

**A:** 临时取消只读权限，执行 checkout 后重新勾选。

### 10.5 Q: 多个项目同时编译会冲突吗？

**A:** 不会。IDF 会在每个项目的 `managed_components/` 目录中创建组件副本，各项目之间相互隔离。

---

## 十一、操作命令速查表

| 操作 | 命令 |
|------|------|
| 初始化仓库 | `git init` |
| 添加文件 | `git add .` |
| 提交 | `git commit -m "message"` |
| 创建分支 | `git checkout -b <分支名>` |
| 切换分支 | `git checkout <分支名>` |
| 打标签 | `git tag -a v1.0.0 -m "description"` |
| 查看标签 | `git tag -l` |
| 查看状态 | `git status` |
| 查看日志 | `git log --oneline` |
| 切换到标签 | `git checkout v1.0.0` |
| 删除标签 | `git tag -d v1.0.0` |

---

## 十二、版本发布检查清单

发布新版本前，请确认：

- [ ] 所有功能已开发完成并测试通过
- [ ] 代码已提交到开发分支
- [ ] 已更新 `idf_component.yml` 中的版本号
- [ ] 已更新 `README.md` 中的文档
- [ ] 已打版本标签并推送到共享仓库
- [ ] 已重新设置文件夹只读权限
- [ ] 已通知团队成员版本更新

---

## 附录：组件版本历史记录模板

| 版本 | 日期 | 说明 | 开发分支 |
|------|------|------|----------|
| v1.0.0 | 2026-06-28 | 基础版本，支持 SSD1306 I2C OLED | feature/v1.0.0 |
| v1.1.0 | 2026-06-29 | 添加 LVGL 移植层 | feature/v1.1.0 |
| v2.0.0 | 2026-06-30 | 添加 SPI TFT、自动文本检测、富文本显示 | feature/v2.0.0 |

---

**文档版本**: v1.0  
**创建日期**: 2026-06-29  
**适用组件**: display_driver, system_scheduler, fonts, wifi_manager