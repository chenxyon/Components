# W25QXX Driver Component

W25QXX系列SPI Flash驱动组件，支持ESP32-S3平台，提供完整的Flash读写操作、FatFs文件系统集成和USB MSC功能。

## 功能特性

| 功能 | 描述 | 标志位 |
|------|------|--------|
| **基础SPI操作** | W25QXX芯片初始化、读写擦除 | 内置 |
| **FatFs文件系统** | 通过FatFs在Flash上创建和管理文件 | `W25QXX_FEATURE_FATFS` |
| **USB MSC** | 将W25QXX模拟为USB闪存盘 | `W25QXX_FEATURE_MSC` |
| **串口CLI** | 通过串口命令交互操作Flash（独立组件 `w25qxx_cli`） | `W25QXX_FEATURE_CLI` |

## 硬件连接

### ESP32-S3与W25QXX引脚对应关系

| W25QXX引脚 | ESP32-S3 GPIO | 信号名称 |
|------------|---------------|----------|
| D0 / SO | GPIO8 | MISO |
| D1 / SI | GPIO7 | MOSI |
| CLK | GPIO6 | SCLK |
| CS | GPIO9 | CS (软件控制) |
| VCC | - | 3.3V |
| GND | - | GND |
| WP | - | 可选，接GND或上拉 |
| HOLD | - | 可选，接GND或上拉 |

### 支持的SPI主机

- `SPI3_HOST` (VSPI) - 推荐使用
- `SPI2_HOST` (HSPI)
- `SPI1_HOST` (FSPI) - 注意：FSPI通常用于内部Flash

## API参考

### 初始化与配置

```c
// 配置结构体
typedef struct {
    int spi_host;           // SPI主机编号 (SPI1_HOST/SPI2_HOST/SPI3_HOST)
    int cs_pin;             // CS引脚编号
    int sclk_pin;           // SCLK引脚编号
    int mosi_pin;           // MOSI引脚编号
    int miso_pin;           // MISO引脚编号
    int spi_mode;           // SPI模式: 0或3，W25QXX推荐使用Mode 3
    uint8_t feature_flags;  // 功能标志位，见w25qxx_feature_flags_t
    spi_device_handle_t spi_handle;  // SPI设备句柄(内部使用)
    w25qxx_info_t info;     // 设备信息(内部使用)
} w25qxx_config_t;

// 功能标志位枚举
typedef enum {
    W25QXX_FEATURE_NONE     = 0x00,  // 无额外功能
    W25QXX_FEATURE_CLI      = 0x01,  // 启用串口命令交互功能
    W25QXX_FEATURE_FATFS    = 0x02,  // 启用FatFs文件系统功能
    W25QXX_FEATURE_MSC      = 0x04,  // 启用USB MSC功能
} w25qxx_feature_flags_t;

// 初始化W25QXX
bool w25qxx_init(w25qxx_config_t *config);

// 反初始化W25QXX
void w25qxx_deinit(w25qxx_config_t *config);
```

### 基础操作

```c
// 读取设备信息
bool w25qxx_read_info(w25qxx_config_t *config, w25qxx_info_t *info);

// 读取数据
bool w25qxx_read(w25qxx_config_t *config, uint32_t addr, uint8_t *data, uint32_t len);

// 写入数据（自动按页写入）
bool w25qxx_write(w25qxx_config_t *config, uint32_t addr, const uint8_t *data, uint32_t len);

// 擦除扇区(4KB)
bool w25qxx_erase_sector(w25qxx_config_t *config, uint32_t addr);

// 擦除块(64KB)
bool w25qxx_erase_block(w25qxx_config_t *config, uint32_t addr);

// 擦除整片芯片
bool w25qxx_erase_chip(w25qxx_config_t *config);

// 等待Flash就绪
void w25qxx_wait_busy(w25qxx_config_t *config);
```

### FatFs磁盘IO驱动

```c
// 注册磁盘驱动
DSTATUS w25qxx_disk_initialize(BYTE pdrv);
DSTATUS w25qxx_disk_status(BYTE pdrv);
DRESULT w25qxx_disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count);
DRESULT w25qxx_disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count);
DRESULT w25qxx_disk_ioctl(BYTE pdrv, BYTE cmd, void *buff);

// 设置磁盘配置
void w25qxx_diskio_set_config(w25qxx_config_t *config);
```

### USB MSC驱动

```c
// MSC操作函数
int32_t msc_read_sectors(uint32_t lba, uint32_t sector_count, uint8_t *buf);
int32_t msc_write_sectors(uint32_t lba, uint32_t sector_count, uint8_t *buf);
uint32_t msc_get_sector_count(void);
uint32_t msc_get_sector_size(void);
void msc_disk_set_config(w25qxx_config_t *config);
```

### 串口CLI命令

启用`W25QXX_FEATURE_CLI`后，通过串口(UART0, 115200波特率)发送命令：

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

### 示例1：基础初始化（仅SPI操作）

```c
#include "w25qxx.h"
#include "w25qxx_spi.h"

w25qxx_config_t w25qxx_cfg = {
    .spi_host = SPI3_HOST,
    .cs_pin = GPIO_NUM_9,
    .miso_pin = GPIO_NUM_8,
    .mosi_pin = GPIO_NUM_7,
    .sclk_pin = GPIO_NUM_6,
    .spi_mode = 3,  // Mode 3 (CPOL=1, CPHA=1)
    .feature_flags = W25QXX_FEATURE_NONE,  // 仅基础功能
};

void app_main(void) {
    if (!w25qxx_init(&w25qxx_cfg)) {
        // 初始化失败
        return;
    }
    
    // 读取数据
    uint8_t data[16];
    w25qxx_read(&w25qxx_cfg, 0, data, sizeof(data));
    
    // 写入数据
    uint8_t write_data[4] = {0x11, 0x22, 0x33, 0x44};
    w25qxx_write(&w25qxx_cfg, 0, write_data, sizeof(write_data));
}
```

### 示例2：启用FatFs和CLI功能

```c
w25qxx_config_t w25qxx_cfg = {
    .spi_host = SPI3_HOST,
    .cs_pin = GPIO_NUM_9,
    .miso_pin = GPIO_NUM_8,
    .mosi_pin = GPIO_NUM_7,
    .sclk_pin = GPIO_NUM_6,
    .spi_mode = 3,
    .feature_flags = W25QXX_FEATURE_FATFS | W25QXX_FEATURE_CLI,
};

void app_main(void) {
    w25qxx_init(&w25qxx_cfg);
    
    // 注册磁盘驱动并挂载FatFs
    ff_diskio_register(0, w25qxx_disk_initialize, w25qxx_disk_status, 
                       w25qxx_disk_read, w25qxx_disk_write, w25qxx_disk_ioctl);
    f_mount(&fs, "0:", 1);
    
    // CLI任务已自动启动，可以通过串口发送命令
}
```

### 示例3：启用所有功能（完整配置）

```c
w25qxx_config_t w25qxx_cfg = {
    .spi_host = SPI3_HOST,
    .cs_pin = GPIO_NUM_9,
    .miso_pin = GPIO_NUM_8,
    .mosi_pin = GPIO_NUM_7,
    .sclk_pin = GPIO_NUM_6,
    .spi_mode = 3,
    .feature_flags = W25QXX_FEATURE_CLI | W25QXX_FEATURE_FATFS | W25QXX_FEATURE_MSC,
};

void app_main(void) {
    w25qxx_init(&w25qxx_cfg);  // 自动初始化所有启用的功能
    
    // 注册磁盘驱动（如果启用了FatFs）
    if (w25qxx_cfg.feature_flags & W25QXX_FEATURE_FATFS) {
        ff_diskio_register(0, w25qxx_disk_initialize, w25qxx_disk_status, 
                           w25qxx_disk_read, w25qxx_disk_write, w25qxx_disk_ioctl);
        f_mount(&fs, "0:", 1);
    }
    
    // 初始化TinyUSB（如果启用了MSC）
    if (w25qxx_cfg.feature_flags & W25QXX_FEATURE_MSC) {
        tinyusb_driver_install(&tusb_cfg);
        xTaskCreate(usb_event_task, "usb_event_task", 4096, NULL, 5, NULL);
    }
    
    // CLI任务已自动启动
}
```

## 支持的W25QXX型号

| 型号 | 容量 | JEDEC ID |
|------|------|----------|
| W25Q16 | 2 MB | 0xEF 0x40 0x15 |
| W25Q32 | 4 MB | 0xEF 0x40 0x16 |
| W25Q64 | 8 MB | 0xEF 0x40 0x17 |
| W25Q128 | 16 MB | 0xEF 0x40 0x18 |
| W25Q256 | 32 MB | 0xEF 0x40 0x19 |
| W25Q512 | 64 MB | 0xEF 0x40 0x20 |
| W25Q1024 | 128 MB | 0xEF 0x40 0x21 |

## 组件目录结构

```
w25qxx_driver/
├── CMakeLists.txt          # 组件构建配置
├── w25qxx.h                # 公共API声明
├── w25qxx.c                # W25QXX核心操作实现
├── w25qxx_spi.h            # SPI驱动声明和配置结构体
├── w25qxx_spi.c            # SPI底层驱动实现
├── w25qxx_diskio.h         # FatFs磁盘IO驱动声明
├── w25qxx_diskio.c         # FatFs磁盘IO驱动实现
├── msc_disk.h              # USB MSC驱动声明
├── msc_disk.c              # USB MSC驱动实现
└── README.md               # 组件文档
```

### 相关组件

| 组件 | 路径 | 描述 |
|------|------|------|
| w25qxx_cli | `components/w25qxx_cli/` | 串口命令行交互组件，需单独引入 |

## 依赖组件

| 组件 | 用途 |
|------|------|
| `driver` | ESP-IDF驱动层，提供SPI和GPIO功能 |
| `fatfs` | FatFs文件系统组件 |
| `tinyusb` | USB MSC功能所需（可选） |

## 配置说明

在`sdkconfig`中需要配置以下项（使用USB MSC时）：

```
CONFIG_TINYUSB_ENABLED=y
CONFIG_TUD_MSC=y
CONFIG_TUD_MSC_BUFSIZE=512
CONFIG_ESP_TINYUSB_ENABLED=y
```

## 注意事项

1. **SPI模式**：W25QXX芯片默认使用SPI Mode 3（CPOL=1, CPHA=1），与STM32参考代码一致。
2. **CS控制**：组件使用软件CS控制，确保CS引脚在SPI事务前后正确切换。
3. **4字节地址模式**：对于W25Q512及以上型号，自动启用4字节地址模式。
4. **扇区大小**：FatFs磁盘IO使用4KB扇区大小，与W25QXX的扇区擦除大小一致。
5. **电源**：W25QXX需要稳定的3.3V电源，建议添加去耦电容。

## 故障排查

### JEDEC ID读取失败（0x00 0x00 0x00）

可能原因：
- SPI引脚连接错误或接触不良
- CS引脚电平不正确（软件CS需要初始化为高电平）
- SPI模式设置错误（应使用Mode 3）
- W25QXX芯片未上电或损坏

### SPI回环测试失败

可能原因：
- MOSI和MISO引脚未正确连接到W25QXX
- 引脚定义错误
- 物理连接问题

### USB MSC无法识别

可能原因：
- TinyUSB未正确初始化
- USB线缆问题
- 设备描述符配置错误
