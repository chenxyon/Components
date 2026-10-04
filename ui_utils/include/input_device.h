#ifndef INPUT_DEVICE_H
#define INPUT_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>

#define KEY_UP      LV_KEY_UP
#define KEY_DOWN    LV_KEY_DOWN
#define KEY_LEFT    LV_KEY_LEFT
#define KEY_RIGHT   LV_KEY_RIGHT
#define KEY_ENTER   LV_KEY_ENTER

#define DOUBLE_CLICK_TIMEOUT_MS 300
#define RESET_COMBINE_HOLD_MS   3000

typedef enum {
    INPUT_EVENT_SINGLE_CLICK,
    INPUT_EVENT_DOUBLE_CLICK,
    INPUT_EVENT_KEY_PRESS,
    INPUT_EVENT_KEY_RELEASE,
} input_event_type_t;

typedef struct {
    input_event_type_t type;
    lv_key_t key;
} input_event_t;

typedef void (*input_event_cb_t)(input_event_t *event);

void keypad_init(input_event_cb_t cb);
void keypad_deinit(void);
void keypad_scan(void);

void touchpad_init(input_event_cb_t cb);
void touchpad_deinit(void);
void touchpad_scan(void);

void input_device_init(void);
void input_device_deinit(void);
void input_device_scan(void);

#ifdef __cplusplus
}
#endif

#endif
