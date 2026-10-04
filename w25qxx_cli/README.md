# W25QXX CLI Component

W25QXX串口命令行交互组件，提供通过串口操作W25QXX Flash的命令行界面。

## 功能特性

- 通过串口(UART0, 115200波特率)发送命令操作Flash
- 支持基础读写擦除操作
- 支持FatFs文件系统操作（挂载、格式化、文件读写、目录浏览）

## 依赖组件

| 组件 | 用途 |
|------|------|
| `w25qxx_driver` | W25QXX基础驱动 |
| `fatfs` | FatFs文件系统组件 |

## API参考

```c
// 设置W25QXX配置（必须在启动任务前调用）
void w25qxx_cli_set_config(w25qxx_config_t *config);

// CLI任务入口函数（通过xTaskCreate创建任务）
void w25qxx_cli_task(void *arg);
```

## CLI命令列表

| 命令 | 说明 | 示例 |
|------|------|------|
| `help` | 显示帮助信息 | `help` |
| `info` | 显示设备信息 | `info` |
| `read addr len` | 读取指定地址的数据 | `read 0 256` |
| `write addr` | 写入数据到指定地址 | `write 0` |
| `erase addr` | 擦除指定扇区 | `erase 0` |
| `read_file path` | 读取文件内容 | `read_file /test.txt` |
| `write_file path` | 写入文件 | `write_file /test.txt` |
| `list [path]` | 列出目录内容 | `list /data` |
| `format` | 格式化磁盘为FAT32 | `format` |
| `mount` | 挂载文件系统 | `mount` |
| `unmount` | 卸载文件系统 | `unmount` |

## 使用示例

```c
#include "w25qxx_cli.h"

// 在main.c中初始化CLI组件
void app_main(void) {
    // 1. 先初始化w25qxx_driver
    w25qxx_init(&w25qxx_cfg);
    
    // 2. 设置CLI配置并创建任务
    w25qxx_cli_set_config(&w25qxx_cfg);
    xTaskCreate(w25qxx_cli_task, "w25qxx_cli_task", 4096, NULL, 5, NULL);
}
```

## 组件目录结构

```
w25qxx_cli/
├── CMakeLists.txt          # 组件构建配置
├── w25qxx_cli.h            # CLI API声明
├── w25qxx_cli.c            # CLI实现
└── README.md               # 组件文档
```

## 注意事项

1. 串口配置：UART0, 115200波特率, 8N1
2. 确保已正确初始化w25qxx_driver后再启动CLI任务
3. 文件系统操作需要先挂载(`mount`)或格式化(`format`)
