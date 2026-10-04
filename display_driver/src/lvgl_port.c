/**
 * @file lvgl_port.c
 * @brief LVGL移植层 - 多屏幕支持
 * @version 1.1.0
 * @date 2026-06-29
 */

#include "lvgl_port.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"

static const char *TAG = "LVGL_PORT";

#define MAX_DISPLAYS 4
#define FLUSH_QUEUE_SIZE 10

typedef struct {
    lv_display_t *display;
    int display_id;
    int32_t x1, y1, x2, y2;
    uint8_t *px_map;
} flush_request_t;

static lv_display_t *lv_displays[MAX_DISPLAYS] = {NULL};
static int display_count = 0;
static uint8_t *display_bufs[MAX_DISPLAYS][2] = {NULL};
static const display_driver_t *display_drivers[MAX_DISPLAYS] = {NULL};
static TaskHandle_t lvgl_task_handle = NULL;

static lvgl_lock_fn_t s_lock_fn = NULL;
static lvgl_unlock_fn_t s_unlock_fn = NULL;

void lvgl_port_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    int display_id = -1;
    for (int i = 0; i < MAX_DISPLAYS; i++) {
        if (lv_displays[i] == display) {
            display_id = i;
            break;
        }
    }
    
    if (display_id < 0 || display_drivers[display_id] == NULL) {
        lv_display_flush_ready(display);
        return;
    }
    
    display_drivers[display_id]->ops->flush(area->x1, area->y1, area->x2, area->y2, px_map);
    lv_display_flush_ready(display);
}

static void lvgl_tick_task(void *arg)
{
    while (1) {
        lv_tick_inc(LVGL_TICK_PERIOD_MS);
        vTaskDelay(pdMS_TO_TICKS(LVGL_TICK_PERIOD_MS));
    }
}

static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task started");
    
    /* 注册任务到 WDT */
    esp_err_t ret = esp_task_wdt_add(NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add LVGL task to WDT: %s", esp_err_to_name(ret));
    }
    
    while (1) {
        lv_timer_handler();
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

lv_display_t *lvgl_port_init_display(display_type_t type, const display_pin_config_t *pins, int display_id)
{
    if (display_id >= MAX_DISPLAYS) {
        ESP_LOGE(TAG, "Display ID %d exceeds max %d", display_id, MAX_DISPLAYS);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Initializing display %d: type=%d", display_id, type);
    
    const display_driver_t *driver = display_driver_find(type);
    if (driver == NULL) {
        ESP_LOGE(TAG, "Driver not found for type %d", type);
        return NULL;
    }
    
    const display_pin_config_t *use_pins = pins;
    if (use_pins == NULL) {
        use_pins = driver->default_pins;
    }
    
    esp_err_t ret = driver->ops->init(type, use_pins);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Driver init failed: %s", esp_err_to_name(ret));
        return NULL;
    }
    
    display_drivers[display_id] = driver;
    
    int width = driver->params->width;
    int height = driver->params->height;
    bool is_monochrome = driver->params->is_monochrome;
    
    ESP_LOGI(TAG, "Display %d: %s (%dx%d, %s)", 
             display_id, driver->params->name, width, height,
             is_monochrome ? "monochrome" : "color");
    
    lv_display_t *display = lv_display_create(width, height);
    if (display == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL display %d", display_id);
        return NULL;
    }
    
    lv_displays[display_id] = display;
    
    if (is_monochrome) {
        /* LVGL 9.x: 单色屏使用 I1 格式（1bit/像素）
         * I1 是索引色格式，LVGL 会在缓冲区前预留 8 字节调色板空间（自动管理） */
        lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);
    } else {
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565_SWAPPED);
    }

    /* 缓冲区大小计算 */
    uint32_t buf_size;
    if (is_monochrome) {
        /* FULL 模式: 全屏 I1 缓冲区
         * stride = lv_draw_buf_width_to_stride(width, I1) = (width+7)/8 字节/行（对齐后）
         * 额外 8 字节用于 I1 调色板（LVGL 自动预留） */
        uint32_t stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_I1);
        buf_size = stride * height + 8;
    } else {
        /* RGB565: 每像素2字节，10行高 */
        buf_size = width * 10 * 2;
    }
    
    display_bufs[display_id][0] = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    display_bufs[display_id][1] = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    
    if (display_bufs[display_id][0] == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer for display %d", display_id);
        lv_display_delete(display);
        return NULL;
    }
    
    if (display_bufs[display_id][1] == NULL) {
        ESP_LOGW(TAG, "Display %d using single buffer (buf_size=%lu)", display_id, (unsigned long)buf_size);
        lv_display_set_buffers(display, display_bufs[display_id][0], NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    } else {
        lv_display_set_buffers(display, display_bufs[display_id][0], display_bufs[display_id][1], buf_size, LV_DISPLAY_RENDER_MODE_FULL);
    }
    
    lv_display_set_flush_cb(display, lvgl_port_flush);
    
    return display;
}

esp_err_t lvgl_port_init_multi(display_type_t *types, const display_pin_config_t **pins, int count)
{
    if (count > MAX_DISPLAYS) {
        ESP_LOGE(TAG, "Display count %d exceeds max %d", count, MAX_DISPLAYS);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Initializing %d displays", count);
    
    lv_init();
    
    for (int i = 0; i < count; i++) {
        lv_display_t *display = lvgl_port_init_display(types[i], pins[i], i);
        if (display == NULL) {
            ESP_LOGE(TAG, "Failed to initialize display %d", i);
            return ESP_FAIL;
        }
    }
    
    display_count = count;
    
    return ESP_OK;
}

esp_err_t lvgl_port_init(display_type_t type, const display_pin_config_t *pins)
{
    ESP_LOGI(TAG, "Initializing LVGL port");
    
    lv_init();
    
    lv_display_t *display = lvgl_port_init_display(type, pins, 0);
    if (display == NULL) {
        ESP_LOGE(TAG, "Failed to initialize LVGL display");
        return ESP_FAIL;
    }
    
    display_count = 1;
    
    xTaskCreate(lvgl_tick_task, "lvgl_tick", 1024, NULL, 10, NULL);
    xTaskCreate(lvgl_task, "lvgl_task", 4096, NULL, 5, &lvgl_task_handle);
    
    ESP_LOGI(TAG, "LVGL port initialized successfully");
    return ESP_OK;
}

esp_err_t lvgl_port_start(void)
{
    ESP_LOGI(TAG, "Starting LVGL tasks");
    
    xTaskCreate(lvgl_tick_task, "lvgl_tick", 1024, NULL, 10, NULL);
    xTaskCreate(lvgl_task, "lvgl_task", 4096, NULL, 5, &lvgl_task_handle);
    
    ESP_LOGI(TAG, "LVGL tasks started");
    return ESP_OK;
}

void lvgl_port_deinit(void)
{
    for (int i = 0; i < display_count; i++) {
        if (display_drivers[i] && display_drivers[i]->ops->deinit) {
            display_drivers[i]->ops->deinit();
        }
        if (display_bufs[i][0]) {
            free(display_bufs[i][0]);
        }
        if (display_bufs[i][1]) {
            free(display_bufs[i][1]);
        }
        if (lv_displays[i]) {
            lv_display_delete(lv_displays[i]);
        }
    }
    
    if (lvgl_task_handle) {
        vTaskDelete(lvgl_task_handle);
    }
    
    display_count = 0;
    
    ESP_LOGI(TAG, "LVGL port deinitialized");
}

lv_display_t *lvgl_port_get_display(void)
{
    return lvgl_port_get_display_by_id(0);
}

lv_display_t *lvgl_port_get_display_by_id(int display_id)
{
    if (display_id >= 0 && display_id < display_count) {
        return lv_displays[display_id];
    }
    return NULL;
}

int lvgl_port_get_display_count(void)
{
    return display_count;
}

void lvgl_port_set_lock_functions(lvgl_lock_fn_t lock_fn, lvgl_unlock_fn_t unlock_fn)
{
    s_lock_fn = lock_fn;
    s_unlock_fn = unlock_fn;
}

lvgl_lock_fn_t lvgl_port_get_lock_fn(void)
{
    return s_lock_fn;
}

lvgl_unlock_fn_t lvgl_port_get_unlock_fn(void)
{
    return s_unlock_fn;
}

const char *lvgl_port_get_version(void)
{
    return LVGL_PORT_VERSION;
}

void lvgl_port_lock(uint32_t timeout_ms)
{
    if (s_lock_fn) {
        s_lock_fn();
    }
}

void lvgl_port_unlock(void)
{
    if (s_unlock_fn) {
        s_unlock_fn();
    }
}