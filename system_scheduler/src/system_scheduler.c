#include "system_scheduler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdint.h>

#ifndef CONFIG_SCHEDULER_MAX_TASKS
#define CONFIG_SCHEDULER_MAX_TASKS 10
#endif
#define MAX_TASKS CONFIG_SCHEDULER_MAX_TASKS

static const char *TAG = "SCHEDULER";

typedef struct {
    scheduler_task_cb_t cb;
    uint32_t interval_ms;
    uint32_t last_run;
    void *arg;
    char name[32];
    bool active;
} scheduler_task_t;

static scheduler_task_t tasks[MAX_TASKS];
static SemaphoreHandle_t task_mutex = NULL;
static SemaphoreHandle_t timer_sem = NULL;
static TaskHandle_t scheduler_task_handle = NULL;
static esp_timer_handle_t timer_handle = NULL;
static scheduler_lock_fn_t lock_fn = NULL;
static scheduler_unlock_fn_t unlock_fn = NULL;
static uint32_t wdt_timeout_ms = 0;
static bool initialized = false;

static void scheduler_timer_callback(void *arg)
{
    xSemaphoreGive(timer_sem);
}

static bool wdt_registered = false;

static void scheduler_task(void *arg)
{
    ESP_LOGI(TAG, "Scheduler task started");

    if (wdt_timeout_ms > 0) {
        esp_err_t ret = esp_task_wdt_add(NULL);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to add scheduler task to WDT: %s", esp_err_to_name(ret));
        } else {
            wdt_registered = true;
            ESP_LOGI(TAG, "Scheduler task registered to WDT");
        }
    }

    while (1) {
        xSemaphoreTake(timer_sem, portMAX_DELAY);

        if (wdt_registered) {
            esp_task_wdt_reset();
        }

        xSemaphoreTake(task_mutex, portMAX_DELAY);

        uint32_t now = esp_timer_get_time() / 1000;
        uint32_t min_next = UINT32_MAX;

        for (int i = 0; i < MAX_TASKS; i++) {
            if (!tasks[i].active) continue;

            if (now >= tasks[i].last_run + tasks[i].interval_ms) {
                if (lock_fn) lock_fn();
                tasks[i].cb(tasks[i].arg);
                if (unlock_fn) unlock_fn();
                tasks[i].last_run = now;
            }

            uint32_t next = tasks[i].last_run + tasks[i].interval_ms - now;
            if (next < min_next) {
                min_next = next;
            }
        }

        xSemaphoreGive(task_mutex);

        if (min_next > 0) {
            esp_timer_stop(timer_handle);
            esp_timer_start_once(timer_handle, min_next * 1000);
        }
    }
}

esp_err_t system_scheduler_init(const scheduler_config_t *config)
{
    if (initialized) {
        ESP_LOGW(TAG, "Scheduler already initialized");
        return ESP_OK;
    }

    if (!config) {
        ESP_LOGE(TAG, "Invalid config");
        return ESP_ERR_INVALID_ARG;
    }

    lock_fn = config->lock_fn;
    unlock_fn = config->unlock_fn;
    wdt_timeout_ms = config->wdt_timeout_ms;

    task_mutex = xSemaphoreCreateMutex();
    timer_sem = xSemaphoreCreateBinary();

    if (!task_mutex || !timer_sem) {
        ESP_LOGE(TAG, "Failed to create semaphores");
        return ESP_ERR_NO_MEM;
    }

    memset(tasks, 0, sizeof(tasks));

    esp_timer_create_args_t timer_args = {
        .callback = scheduler_timer_callback,
        .name = "scheduler_timer"
    };
    esp_timer_create(&timer_args, &timer_handle);

    BaseType_t ret = xTaskCreatePinnedToCore(
        scheduler_task,
        "scheduler",
        config->task_stack_size,
        NULL,
        config->task_priority,
        &scheduler_task_handle,
        1
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create scheduler task");
        return ESP_FAIL;
    }

    initialized = true;
    ESP_LOGI(TAG, "Scheduler initialized (WDT timeout: %lu ms)", wdt_timeout_ms);

    return ESP_OK;
}

esp_err_t system_scheduler_add_task(scheduler_task_cb_t cb, uint32_t interval_ms, const char *name, void *arg)
{
    if (!initialized || !cb) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(task_mutex, portMAX_DELAY);

    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!tasks[i].active) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        xSemaphoreGive(task_mutex);
        ESP_LOGE(TAG, "Task list full");
        return ESP_ERR_NO_MEM;
    }

    tasks[slot].cb = cb;
    tasks[slot].interval_ms = interval_ms;
    tasks[slot].last_run = esp_timer_get_time() / 1000;
    tasks[slot].arg = arg;
    strncpy(tasks[slot].name, name ? name : "unknown", sizeof(tasks[slot].name) - 1);
    tasks[slot].active = true;

    xSemaphoreGive(task_mutex);

    xSemaphoreGive(timer_sem);

    ESP_LOGI(TAG, "Added task: %s (interval: %lu ms)", name, interval_ms);
    return ESP_OK;
}

esp_err_t system_scheduler_remove_task(scheduler_task_cb_t cb, void *arg)
{
    if (!initialized || !cb) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(task_mutex, portMAX_DELAY);

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].active && tasks[i].cb == cb && tasks[i].arg == arg) {
            tasks[i].active = false;
            ESP_LOGI(TAG, "Removed task: %s", tasks[i].name);
            xSemaphoreGive(task_mutex);
            return ESP_OK;
        }
    }

    xSemaphoreGive(task_mutex);
    ESP_LOGW(TAG, "Task not found");
    return ESP_ERR_NOT_FOUND;
}

void system_scheduler_feed_watchdog(void)
{
    if (wdt_registered) {
        esp_task_wdt_reset();
    }
}

void system_scheduler_lock(void)
{
    if (lock_fn) lock_fn();
}

void system_scheduler_unlock(void)
{
    if (unlock_fn) unlock_fn();
}