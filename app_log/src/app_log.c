#include "app_log.h"
#include "esp_log.h"
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define LOG_BUFFER_SIZE 4096
#define LOG_LINE_MAX    128
#define NVS_NAMESPACE   "app_log"
#define AUTO_SAVE_INTERVAL 10

static char g_log_buffer[LOG_BUFFER_SIZE];
static int g_log_offset = 0;
static int g_log_count = 0;
static int g_unsaved_count = 0;
static bool g_auto_save_enabled = false;
static SemaphoreHandle_t g_log_mutex;
static vprintf_like_t g_original_vprintf = NULL;

static int log_output_callback(const char *fmt, va_list args) {
    /* 同时输出到串口 */
    if (g_original_vprintf) {
        g_original_vprintf(fmt, args);
    }

    char line[LOG_LINE_MAX];
    int len = vsnprintf(line, LOG_LINE_MAX, fmt, args);
    if (len <= 0) return len;

    if (g_log_mutex) xSemaphoreTake(g_log_mutex, portMAX_DELAY);

    if (g_log_offset + len + 1 >= LOG_BUFFER_SIZE) {
        g_log_offset = 0;
    }
    memcpy(g_log_buffer + g_log_offset, line, len);
    g_log_offset += len;
    g_log_buffer[g_log_offset] = '\0';
    g_log_count++;
    g_unsaved_count++;

    if (g_auto_save_enabled && g_unsaved_count >= AUTO_SAVE_INTERVAL) {
        app_log_save_to_nvs();
        g_unsaved_count = 0;
    }

    if (g_log_mutex) xSemaphoreGive(g_log_mutex);
    return len;
}

void app_log_init(void) {
    g_log_mutex = xSemaphoreCreateMutex();
    memset(g_log_buffer, 0, LOG_BUFFER_SIZE);
    g_log_offset = 0;
    g_log_count = 0;
    g_unsaved_count = 0;

    /* 保存原始vprintf，确保日志同时输出到串口 */
    g_original_vprintf = esp_log_set_vprintf((vprintf_like_t)log_output_callback);

    esp_log_level_set("KEYBOARD", ESP_LOG_DEBUG);
    esp_log_level_set("BUZZER", ESP_LOG_DEBUG);
    esp_log_level_set("OLED", ESP_LOG_DEBUG);
    esp_log_level_set("MAIN", ESP_LOG_DEBUG);
    esp_log_level_set("WIFI_HTTP", ESP_LOG_DEBUG);

    app_log_load_from_nvs();
    app_log_auto_save(true);

    ESP_LOGI("APP_LOG", "Log component initialized, buffer %d bytes, auto-save ON", LOG_BUFFER_SIZE);
}

const char *app_log_get_buffer(void) {
    return g_log_buffer;
}

int app_log_get_count(void) {
    return g_log_count;
}

void app_log_save_to_nvs(void) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) return;

    nvs_set_str(handle, "log_buf", g_log_buffer);
    nvs_set_i32(handle, "log_count", g_log_count);
    nvs_set_i32(handle, "log_offset", g_log_offset);
    nvs_commit(handle);
    nvs_close(handle);
}

void app_log_load_from_nvs(void) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) return;

    size_t required_size = LOG_BUFFER_SIZE;
    ret = nvs_get_str(handle, "log_buf", g_log_buffer, &required_size);
    if (ret == ESP_OK) {
        g_log_offset = strlen(g_log_buffer);
        int32_t count = 0;
        nvs_get_i32(handle, "log_count", &count);
        g_log_count = count;
        ESP_LOGI("APP_LOG", "Restored %d log entries from NVS", count);
    }

    nvs_close(handle);
}

void app_log_auto_save(bool enable) {
    g_auto_save_enabled = enable;
}
