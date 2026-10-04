#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdbool.h>

#define APP_LOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define APP_LOGW(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define APP_LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define APP_LOGD(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#define APP_LOGV(tag, fmt, ...) ESP_LOGV(tag, fmt, ##__VA_ARGS__)

void app_log_init(void);
const char *app_log_get_buffer(void);
int app_log_get_count(void);
void app_log_save_to_nvs(void);
void app_log_load_from_nvs(void);
void app_log_auto_save(bool enable);

#endif
