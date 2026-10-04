#include "input_device.h"
#include "ui_manager.h"
#include <string.h>

static input_event_cb_t g_event_cb = NULL;

static lv_indev_t *g_keypad_indev = NULL;
static lv_indev_t *g_touchpad_indev = NULL;

static lv_group_t *g_input_group = NULL;

static uint32_t g_last_click_time = 0;
static bool g_first_click_detected = false;

static uint32_t g_reset_hold_start = 0;
static bool g_reset_holding = false;

static uint32_t get_current_time_ms(void) {
    return lv_tick_get();
}

static uint32_t get_physical_key_pressed(void) {
    return 0;
}

static bool is_double_click_detected(void) {
    uint32_t now = get_current_time_ms();
    if (g_first_click_detected) {
        if (now - g_last_click_time <= DOUBLE_CLICK_TIMEOUT_MS) {
            g_first_click_detected = false;
            return true;
        }
        g_first_click_detected = false;
    }
    return false;
}

static bool is_reset_combination_pressed(void) {
    return false;
}

static void keypad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    uint32_t key = get_physical_key_pressed();
    
    if (is_double_click_detected()) {
        data->key = LV_KEY_ENTER;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (key != 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = key;
        
        if (key == KEY_ENTER) {
            uint32_t now = get_current_time_ms();
            if (!g_first_click_detected) {
                g_first_click_detected = true;
                g_last_click_time = now;
            }
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    
    if (is_reset_combination_pressed()) {
        if (!g_reset_holding) {
            g_reset_holding = true;
            g_reset_hold_start = get_current_time_ms();
        } else {
            if (get_current_time_ms() - g_reset_hold_start >= RESET_COMBINE_HOLD_MS) {
                input_event_t event = {
                    .type = INPUT_EVENT_KEY_PRESS,
                    .key = LV_KEY_ENTER
                };
                if (g_event_cb != NULL) {
                    g_event_cb(&event);
                }
                g_reset_holding = false;
            }
        }
    } else {
        g_reset_holding = false;
    }
}

static int g_touch_start_x = 0;
static int g_touch_start_y = 0;
static bool g_touch_scrolling = false;

static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    int16_t rel_x = 0, rel_y = 0;
    bool is_pressed = false;
    
    data->state = is_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    
    if (is_pressed) {
        if (!g_touch_scrolling) {
            g_touch_start_x = data->point.x;
            g_touch_start_y = data->point.y;
            g_touch_scrolling = true;
        }
    } else {
        if (g_touch_scrolling) {
            int delta_x = data->point.x - g_touch_start_x;
            int delta_y = data->point.y - g_touch_start_y;
            
            if (abs(delta_x) > abs(delta_y) && abs(delta_x) > 50) {
                if (delta_x > 0) {
                    ui_manager_prev_page();
                } else {
                    ui_manager_next_page();
                }
            }
            
            g_touch_scrolling = false;
        }
    }
    
    data->enc_diff = rel_x;
}

void keypad_init(input_event_cb_t cb) {
    g_input_group = lv_group_create();
    
    g_keypad_indev = lv_indev_create();
    lv_indev_set_type(g_keypad_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(g_keypad_indev, keypad_read);
    lv_indev_set_group(g_keypad_indev, g_input_group);
    
    g_event_cb = cb;
}

void keypad_deinit(void) {
    if (g_keypad_indev != NULL) {
        lv_indev_delete(g_keypad_indev);
        g_keypad_indev = NULL;
    }
    if (g_input_group != NULL) {
        lv_group_delete(g_input_group);
        g_input_group = NULL;
    }
}

void keypad_scan(void) {
}

void touchpad_init(input_event_cb_t cb) {
    g_touchpad_indev = lv_indev_create();
    lv_indev_set_type(g_touchpad_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(g_touchpad_indev, touchpad_read);
    
    g_event_cb = cb;
}

void touchpad_deinit(void) {
    if (g_touchpad_indev != NULL) {
        lv_indev_delete(g_touchpad_indev);
        g_touchpad_indev = NULL;
    }
}

void touchpad_scan(void) {
}

void input_device_init(void) {
    #if ENABLE_KEYPAD_INPUT
    keypad_init(g_event_cb);
    #endif
    
    #if ENABLE_TOUCHPAD_INPUT
    touchpad_init(g_event_cb);
    #endif
}

void input_device_deinit(void) {
    #if ENABLE_KEYPAD_INPUT
    keypad_deinit();
    #endif
    
    #if ENABLE_TOUCHPAD_INPUT
    touchpad_deinit();
    #endif
}

void input_device_scan(void) {
    #if ENABLE_KEYPAD_INPUT
    keypad_scan();
    #endif
    
    #if ENABLE_TOUCHPAD_INPUT
    touchpad_scan();
    #endif
}