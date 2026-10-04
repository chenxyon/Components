#ifndef UI_RESET_H
#define UI_RESET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define RESET_HOLD_DURATION_MS 3000

typedef void (*reset_trigger_cb_t)(void);

void ui_reset_init(reset_trigger_cb_t cb);
void ui_reset_deinit(void);
void ui_reset_check(void);
bool ui_reset_is_triggered(void);

#ifdef __cplusplus
}
#endif

#endif
