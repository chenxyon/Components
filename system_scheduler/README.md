# System Scheduler 组件 v1.0

## 简介

System Scheduler 是一个独立的事件驱动调度器组件，基于 esp_timer 和 FreeRTOS 实现，支持任务定时调度和看门狗管理。该组件通过函数指针注入锁机制，与显示驱动完全解耦，可独立使用。

## 功能特性

- **事件驱动**：基于 esp_timer 单次触发，到期后发送通知给工作线程
- **任务管理**：支持添加、移除定时任务
- **看门狗集成**：工作线程自动喂狗，防止回调卡死
- **锁机制注入**：通过函数指针注入锁/解锁函数，与 LVGL 等框架无缝集成
- **线程安全**：使用互斥量保护任务链表

## 文件结构

```
system_scheduler/
├── include/
│   └── system_scheduler.h      # 调度器接口定义
├── src/
│   └── system_scheduler.c      # 调度器实现
├── CMakeLists.txt
├── idf_component.yml
└── README.md
```

## API 参考

### 数据结构

```c
typedef void (*scheduler_lock_fn_t)(void);
typedef void (*scheduler_unlock_fn_t)(void);
typedef void (*scheduler_task_cb_t)(void *arg);

typedef struct {
    scheduler_lock_fn_t lock_fn;          // 锁函数（可为NULL）
    scheduler_unlock_fn_t unlock_fn;      // 解锁函数（可为NULL）
    uint32_t wdt_timeout_ms;              // 看门狗超时时间（0=禁用）
    uint32_t tick_interval_ms;            // 内部精度（实际由esp_timer驱动）
    int task_priority;                    // 工作线程优先级
    int task_stack_size;                  // 工作线程栈大小（字节）
} scheduler_config_t;
```

### 函数接口

```c
// 初始化调度器
esp_err_t system_scheduler_init(const scheduler_config_t *config);

// 添加定时任务
esp_err_t system_scheduler_add_task(scheduler_task_cb_t cb, uint32_t interval_ms, const char *name, void *arg);

// 移除定时任务
esp_err_t system_scheduler_remove_task(scheduler_task_cb_t cb, void *arg);

// 手动喂狗（一般无需外部调用）
void system_scheduler_feed_watchdog(void);

// 调度器锁/解锁（供任务回调使用）
void system_scheduler_lock(void);
void system_scheduler_unlock(void);
```

## 使用示例

### 基础使用

```c
#include "system_scheduler.h"

static void update_clock(void *arg) {
    lv_obj_t *label = (lv_obj_t *)arg;
    // 更新时钟显示...
}

void app_main(void) {
    scheduler_config_t cfg = {
        .lock_fn = NULL,                    // 不使用锁
        .unlock_fn = NULL,
        .wdt_timeout_ms = 5000,             // 5秒看门狗超时
        .task_priority = 5,
        .task_stack_size = 4096
    };
    system_scheduler_init(&cfg);
    
    // 添加定时任务（每秒执行）
    system_scheduler_add_task(update_clock, 1000, "clock", label);
}
```

### 与 LVGL 集成

```c
#include "system_scheduler.h"
#include "lvgl_port.h"

static void lvgl_lock(void) {
    lvgl_port_lock(portMAX_DELAY);
}

static void lvgl_unlock(void) {
    lvgl_port_unlock();
}

void app_main(void) {
    // 初始化显示和LVGL...
    
    scheduler_config_t cfg = {
        .lock_fn = lvgl_lock,
        .unlock_fn = lvgl_unlock,
        .wdt_timeout_ms = 5000,
        .task_priority = 5,
        .task_stack_size = 4096
    };
    system_scheduler_init(&cfg);
    
    // 添加任务，调度器会自动加锁/解锁
    system_scheduler_add_task(update_ui, 1000, "ui_update", ui_obj);
}
```

## 配置选项

通过 `idf.py menuconfig` 配置：

```
ChenYong Component Suite → System Scheduler
├── Maximum Number of Tasks (10)        # 最大任务数
├── Watchdog Timeout (5000 ms)          # 看门狗超时时间
├── Scheduler Task Priority (5)         # 任务优先级
└── Scheduler Task Stack Size (4096)    # 任务栈大小
```

## 工作原理

```
┌─────────────────────────────────────────────────────────┐
│                    esp_timer (单次触发)                  │
│                         │                               │
│                         ▼                               │
│              到期后发送通知给工作线程                      │
│                         │                               │
│                         ▼                               │
│          ┌─────────────────────────────┐               │
│          │     工作线程 (FreeRTOS)      │               │
│          │     ┌─────────────────┐     │               │
│          │     │  遍历任务链表    │     │               │
│          │     │  执行到期回调    │     │               │
│          │     │  (自动加锁/解锁) │     │               │
│          │     └────────┬────────┘     │               │
│          │              │               │               │
│          │              ▼               │               │
│          │     ┌─────────────────┐     │               │
│          │     │   计算下次触发   │     │               │
│          │     │   重新设置定时器 │     │               │
│          │     └────────┬────────┘     │               │
│          │              │               │               │
│          │              ▼               │               │
│          │     ┌─────────────────┐     │               │
│          │     │   esp_task_wdt  │     │               │
│          │     │   _reset() 喂狗 │     │               │
│          │     └─────────────────┘     │               │
│          └─────────────────────────────┘               │
└─────────────────────────────────────────────────────────┘
```

## 注意事项

- **回调阻塞**：调度器回调中禁止使用阻塞函数（如 `vTaskDelay`），否则会导致工作线程卡死，看门狗复位
- **锁机制**：如果注入了锁函数，调度器会在执行回调前后自动调用锁/解锁
- **看门狗**：当 `wdt_timeout_ms` 设置为 0 时，禁用看门狗功能
- **任务数量**：最大任务数由配置决定，超出限制会返回错误

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0.0 | 2026-06-29 | 初始版本，支持事件驱动调度和看门狗管理 |

## 许可证

MIT License