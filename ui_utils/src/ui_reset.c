#include "ui_reset.h"
#include "settings_ui.h"
#include "ui_manager.h"

static reset_trigger_cb_t g_reset_cb = NULL;
static bool g_is_triggered = false;
static bool g_is_holding = false;
static uint32_t g_hold_start_time = 0;

static bool check_reset_combination(void) {
    return false;
}

static uint32_t get_current_time_ms(void) {
    return 0;
}

static void save_settings_to_flash(void) {
}

void ui_reset_init(reset_trigger_cb_t cb) {
    g_reset_cb = cb;
    g_is_triggered = false;
    g_is_holding = false;
}

void ui_reset_deinit(void) {
    g_reset_cb = NULL;
}

void ui_reset_check(void) {
    if (check_reset_combination()) {
        if (!g_is_holding) {
            g_is_holding = true;
            g_hold_start_time = get_current_time_ms();
        } else {
            uint32_t elapsed = get_current_time_ms() - g_hold_start_time;
            if (elapsed >= RESET_HOLD_DURATION_MS) {
                g_is_triggered = true;
                g_is_holding = false;
                
                settings_ui_reset_to_defaults();
                save_settings_to_flash();
                
                if (g_reset_cb != NULL) {
                    g_reset_cb();
                }
                
                ui_manager_notify_event(UI_EVENT_RESET_TRIGGERED);
            }
        }
    } else {
        g_is_holding = false;
    }
}

bool ui_reset_is_triggered(void) {
    return g_is_triggered;
}
