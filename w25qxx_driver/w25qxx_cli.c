#include "w25qxx_cli.h"
#include "w25qxx.h"
#include "w25qxx_diskio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "ff.h"

static const char *TAG = "w25qxx_cli";
static w25qxx_config_t *s_w25qxx_cfg = NULL;
static FATFS s_fs;

#define CLI_UART_NUM UART_NUM_0
#define CLI_BUFFER_SIZE 256
#define CLI_BAUD_RATE 115200

static void cli_print(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
}

static void cli_print_hex(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (i > 0 && i % 16 == 0) cli_print("\n");
        if (i % 16 == 0) cli_print("%04X: ", i);
        cli_print("%02X ", data[i]);
    }
    cli_print("\n");
}

static void cli_cmd_help(void) {
    cli_print("\n=== W25QXX CLI Commands ===\n");
    cli_print("help          - Show this help message\n");
    cli_print("info          - Show W25QXX device info\n");
    cli_print("read addr len - Read data from flash (addr in hex, len in decimal)\n");
    cli_print("write addr    - Write data to flash (enter data after command)\n");
    cli_print("erase addr    - Erase sector at address\n");
    cli_print("read_file path - Read file content\n");
    cli_print("write_file path - Write file (enter content after command)\n");
    cli_print("list [path]   - List directory contents\n");
    cli_print("format        - Format W25QXX with FAT32\n");
    cli_print("mount         - Mount FatFs file system\n");
    cli_print("unmount       - Unmount FatFs file system\n");
    cli_print("===========================\n");
}

static void cli_cmd_info(void) {
    if (!s_w25qxx_cfg) {
        cli_print("Error: W25QXX not initialized\n");
        return;
    }
    cli_print("\n=== W25QXX Device Info ===\n");
    cli_print("Manufacturer: 0x%02X\n", s_w25qxx_cfg->info.manufacturer_id);
    cli_print("Model: 0x%02X\n", s_w25qxx_cfg->info.model);
    cli_print("Total Size: %u MB\n", (unsigned int)(s_w25qxx_cfg->info.total_size / (1024 * 1024)));
    cli_print("Sector Count: %u\n", s_w25qxx_cfg->info.sector_count);
    cli_print("Block Count: %u\n", s_w25qxx_cfg->info.block_count);
    cli_print("4-byte Address Mode: %s\n", s_w25qxx_cfg->info.addr_4byte ? "Yes" : "No");
    cli_print("===========================\n");
}

static void cli_cmd_read(char *args) {
    if (!s_w25qxx_cfg) {
        cli_print("Error: W25QXX not initialized\n");
        return;
    }
    
    uint32_t addr, len;
    if (sscanf(args, "%x %u", &addr, &len) != 2) {
        cli_print("Usage: read <addr> <len>\n");
        cli_print("Example: read 0 256\n");
        return;
    }
    
    if (len > 4096) {
        cli_print("Error: Maximum read length is 4096 bytes\n");
        return;
    }
    
    uint8_t *buf = malloc(len);
    if (!buf) {
        cli_print("Error: Memory allocation failed\n");
        return;
    }
    
    if (w25qxx_read(s_w25qxx_cfg, addr, buf, len)) {
        cli_print("Read %u bytes from 0x%08X:\n", len, addr);
        cli_print_hex(buf, len);
    } else {
        cli_print("Error: Read failed\n");
    }
    free(buf);
}

static void cli_cmd_write(char *args) {
    if (!s_w25qxx_cfg) {
        cli_print("Error: W25QXX not initialized\n");
        return;
    }
    
    uint32_t addr;
    if (sscanf(args, "%x", &addr) != 1) {
        cli_print("Usage: write <addr>\n");
        cli_print("Example: write 0\n");
        return;
    }
    
    cli_print("Enter data to write (max 256 bytes, press Enter to finish):\n");
    
    uint8_t data[256] = {0};
    int len = 0;
    char c;
    
    while (len < 255) {
        if (scanf("%c", &c) != 1) break;
        if (c == '\n') break;
        data[len++] = c;
    }
    
    if (len == 0) {
        cli_print("Error: No data entered\n");
        return;
    }
    
    cli_print("Writing %d bytes to 0x%08X...\n", len, addr);
    
    if (w25qxx_write(s_w25qxx_cfg, addr, data, len)) {
        cli_print("Write successful\n");
    } else {
        cli_print("Error: Write failed\n");
    }
}

static void cli_cmd_erase(char *args) {
    if (!s_w25qxx_cfg) {
        cli_print("Error: W25QXX not initialized\n");
        return;
    }
    
    uint32_t addr;
    if (sscanf(args, "%x", &addr) != 1) {
        cli_print("Usage: erase <addr>\n");
        cli_print("Example: erase 0\n");
        return;
    }
    
    cli_print("Erasing sector at 0x%08X...\n", addr);
    
    if (w25qxx_erase_sector(s_w25qxx_cfg, addr)) {
        cli_print("Erase successful\n");
    } else {
        cli_print("Error: Erase failed\n");
    }
}

static void cli_cmd_read_file(char *args) {
    if (!s_w25qxx_cfg) {
        cli_print("Error: W25QXX not initialized\n");
        return;
    }
    
    /* 用 snprintf 替代 strcat：args 最长可达 224 字节，直接拼接会溢出 path */
    char path[128];
    snprintf(path, sizeof(path), "0:%s", args);

    FIL file;
    FRESULT ret = f_open(&file, path, FA_READ);
    if (ret != FR_OK) {
        cli_print("Error: Failed to open file '%s' (error %d)\n", path, ret);
        return;
    }
    
    char buffer[512] = {0};
    UINT bytes_read;
    ret = f_read(&file, buffer, sizeof(buffer) - 1, &bytes_read);
    
    if (ret == FR_OK) {
        cli_print("Read %u bytes from '%s':\n", bytes_read, path);
        cli_print("%s\n", buffer);
    } else {
        cli_print("Error: Read failed (error %d)\n", ret);
    }
    
    f_close(&file);
}

static void cli_cmd_write_file(char *args) {
    if (!s_w25qxx_cfg) {
        cli_print("Error: W25QXX not initialized\n");
        return;
    }
    
    /* 用 snprintf 替代 strcat：防止 args 超长导致栈溢出 */
    char path[128];
    snprintf(path, sizeof(path), "0:%s", args);

    cli_print("Enter file content (press Enter to finish):\n");
    
    char content[512] = {0};
    int len = 0;
    char c;
    
    while (len < 511) {
        if (scanf("%c", &c) != 1) break;
        if (c == '\n') break;
        content[len++] = c;
    }
    
    if (len == 0) {
        cli_print("Error: No content entered\n");
        return;
    }
    
    FIL file;
    FRESULT ret = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (ret != FR_OK) {
        cli_print("Error: Failed to create file '%s' (error %d)\n", path, ret);
        return;
    }
    
    UINT bytes_written;
    ret = f_write(&file, content, len, &bytes_written);
    
    if (ret == FR_OK) {
        cli_print("Written %u bytes to '%s'\n", bytes_written, path);
    } else {
        cli_print("Error: Write failed (error %d)\n", ret);
    }
    
    f_close(&file);
}

static void cli_cmd_list(char *args) {
    if (!s_w25qxx_cfg) {
        cli_print("Error: W25QXX not initialized\n");
        return;
    }
    
    /* 用 snprintf 替代 strcat：防止 args 超长导致栈溢出 */
    char path[128];
    snprintf(path, sizeof(path), "0:%s", args);

    FRESULT ret;
    DIR dir;
    FILINFO fno;
    
    ret = f_opendir(&dir, path);
    if (ret != FR_OK) {
        cli_print("Error: Failed to open directory '%s' (error %d)\n", path, ret);
        return;
    }
    
    cli_print("\nDirectory listing for '%s':\n", path);
    cli_print("--------------------------------\n");
    
    while (1) {
        ret = f_readdir(&dir, &fno);
        if (ret != FR_OK || fno.fname[0] == 0) break;
        
        if (fno.fattrib & AM_DIR) {
            cli_print("[DIR]  %s\n", fno.fname);
        } else {
            cli_print("      %10lu  %s\n", fno.fsize, fno.fname);
        }
    }
    
    cli_print("--------------------------------\n");
    f_closedir(&dir);
}

static void cli_cmd_format(void) {
    if (!s_w25qxx_cfg) {
        cli_print("Error: W25QXX not initialized\n");
        return;
    }
    
    cli_print("Formatting W25QXX with FAT32...\n");
    
    MKFS_PARM mkfs_param = {
        .fmt = FM_FAT32,
        .n_fat = 1,
        .align = 0,
        .n_root = 0,
        .au_size = 0,
    };
    
    FRESULT ret = f_mkfs("0:", &mkfs_param, NULL, 0);
    if (ret == FR_OK) {
        cli_print("Format successful\n");
    } else {
        cli_print("Error: Format failed (error %d)\n", ret);
    }
}

static void cli_cmd_mount(void) {
    if (!s_w25qxx_cfg) {
        cli_print("Error: W25QXX not initialized\n");
        return;
    }
    
    cli_print("Mounting FatFs file system...\n");
    
    FRESULT ret = f_mount(&s_fs, "0:", 1);
    if (ret == FR_OK) {
        cli_print("Mount successful\n");
    } else {
        cli_print("Error: Mount failed (error %d)\n", ret);
        cli_print("Try 'format' command first\n");
    }
}

static void cli_cmd_unmount(void) {
    cli_print("Unmounting FatFs file system...\n");
    
    FRESULT ret = f_mount(NULL, "0:", 1);
    if (ret == FR_OK) {
        cli_print("Unmount successful\n");
    } else {
        cli_print("Error: Unmount failed (error %d)\n", ret);
    }
}

void w25qxx_cli_task(void *arg) {
    ESP_LOGI(TAG, "W25QXX CLI task started");
    
    char input[CLI_BUFFER_SIZE];
    char cmd[32];
    char args[CLI_BUFFER_SIZE - 32] = {0};  /* 必须初始化：无参数命令下 sscanf 不会写入，
                                             未初始化会导致后续读取栈垃圾并溢出 */
    
    cli_print("\nW25QXX CLI Ready. Type 'help' for commands.\n");
    cli_print("> ");
    
    while (1) {
        // 读取输入
        int len = 0;
        while (len < CLI_BUFFER_SIZE - 1) {
            char c;
            if (scanf("%c", &c) != 1) {
                vTaskDelay(10 / portTICK_PERIOD_MS);
                continue;
            }
            
            if (c == '\n') break;
            if (c == '\r') continue;
            
            // 退格处理
            if (c == '\b' && len > 0) {
                len--;
                cli_print("\b \b");
                continue;
            }
            
            input[len++] = c;
            cli_print("%c", c);
        }
        input[len] = '\0';
        cli_print("\n");
        
        // 解析命令
        args[0] = '\0';  /* 每轮清空，避免上一条命令的参数残留 */
        if (sscanf(input, "%31s %[^\n]", cmd, args) < 1) {
            cli_print("> ");
            continue;
        }
        
        // 执行命令
        if (strcmp(cmd, "help") == 0) {
            cli_cmd_help();
        } else if (strcmp(cmd, "info") == 0) {
            cli_cmd_info();
        } else if (strcmp(cmd, "read") == 0) {
            cli_cmd_read(args);
        } else if (strcmp(cmd, "write") == 0) {
            cli_cmd_write(args);
        } else if (strcmp(cmd, "erase") == 0) {
            cli_cmd_erase(args);
        } else if (strcmp(cmd, "read_file") == 0) {
            cli_cmd_read_file(args);
        } else if (strcmp(cmd, "write_file") == 0) {
            cli_cmd_write_file(args);
        } else if (strcmp(cmd, "list") == 0) {
            cli_cmd_list(args);
        } else if (strcmp(cmd, "format") == 0) {
            cli_cmd_format();
        } else if (strcmp(cmd, "mount") == 0) {
            cli_cmd_mount();
        } else if (strcmp(cmd, "unmount") == 0) {
            cli_cmd_unmount();
        } else {
            cli_print("Unknown command: %s\n", cmd);
            cli_print("Type 'help' for available commands\n");
        }
        
        cli_print("> ");
    }
}

void w25qxx_cli_set_config(w25qxx_config_t *config) {
    s_w25qxx_cfg = config;
}
