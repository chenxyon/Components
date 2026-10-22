/* 唤醒引擎：ESP-SR WakeNet (offline) -- interface placeholder
 *
 * Three steps needed before this engine can be enabled:
 *   1. Enable PSRAM (models must live in PSRAM)
 *   2. Add espressif/esp-sr component + a wake word model
 *   3. Implement the three functions below via esp_afe + esp_gnn_wakeup
 *
 * @note Custom wake words need cloud training (ESP-SR Model Maker).
 *       Keep using the online engine if the wake word may change.
 */
#include "esp_log.h"

#include "kws_engine.h"

static const char *TAG = "KWS_ESP_SR";

static esp_err_t sr_init(void)
{
    ESP_LOGE(TAG, "ESP-SR engine not enabled yet: need PSRAM + esp-sr + wake model");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t sr_start(void) { return ESP_ERR_NOT_SUPPORTED; }
static esp_err_t sr_stop(void)  { return ESP_ERR_NOT_SUPPORTED; }
static bool      sr_busy(void)  { return false; }

const kws_engine_t kws_engine_esp_sr = {
    .name  = "esp-sr",
    .init  = sr_init,
    .start = sr_start,
    .stop  = sr_stop,
    .busy  = sr_busy,
};
