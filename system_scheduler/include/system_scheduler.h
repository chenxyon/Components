#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*scheduler_lock_fn_t)(void);
typedef void (*scheduler_unlock_fn_t)(void);
typedef void (*scheduler_task_cb_t)(void *arg);

typedef struct {
    scheduler_lock_fn_t lock_fn;
    scheduler_unlock_fn_t unlock_fn;
    uint32_t wdt_timeout_ms;
    uint32_t tick_interval_ms;
    int task_priority;
    int task_stack_size;
} scheduler_config_t;

esp_err_t system_scheduler_init(const scheduler_config_t *config);
esp_err_t system_scheduler_add_task(scheduler_task_cb_t cb, uint32_t interval_ms, const char *name, void *arg);
esp_err_t system_scheduler_remove_task(scheduler_task_cb_t cb, void *arg);
void system_scheduler_feed_watchdog(void);
void system_scheduler_lock(void);
void system_scheduler_unlock(void);

#ifdef __cplusplus
}
#endif