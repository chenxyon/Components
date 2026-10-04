#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>

typedef uint32_t app_id_t;

typedef enum {
    ICON_TYPE_NONE = 0,
    ICON_TYPE_IMAGE,
    ICON_TYPE_SYMBOL,
} icon_type_t;

typedef enum {
    DESKTOP_LAYOUT_2x2 = 0,  
    DESKTOP_LAYOUT_2x3,      
    DESKTOP_LAYOUT_3x3,      
    DESKTOP_LAYOUT_4x2,      
    DESKTOP_LAYOUT_4x3,      
    DESKTOP_LAYOUT_MAX,
} desktop_layout_t;

typedef struct {
    int cols;
    int rows;
    int btn_width;
    int btn_height;
    int icon_size;
} desktop_layout_info_t;

typedef struct {
    app_id_t id;
    const char *name;
    icon_type_t icon_type;
    const void *icon;
    void (*launch_cb)(void);
    int8_t visible;
} app_desc_t;

typedef enum {
    UI_EVENT_RESET_TRIGGERED = 0x8000,
    UI_EVENT_PAGE_CHANGED,
} ui_event_t;

#define MAX_APP_COUNT 30

void ui_manager_init(void);
void ui_manager_register_app(const app_desc_t *app);
void ui_manager_unregister_app(app_id_t id);
void ui_manager_launch_app(app_id_t id);
void ui_manager_return_to_desktop(void);
void ui_manager_notify_event(ui_event_t event);

bool ui_manager_is_app_registered(app_id_t id);
bool ui_manager_get_app_visible(app_id_t id);
void ui_manager_set_app_visible(app_id_t id, bool visible);
app_id_t get_current_launching_app_id(void);

desktop_layout_t ui_manager_get_layout(void);
void ui_manager_set_layout(desktop_layout_t layout);
int ui_manager_get_current_page(void);
int ui_manager_get_total_pages(void);
void ui_manager_set_page(int page);
void ui_manager_next_page(void);
void ui_manager_prev_page(void);

#ifdef __cplusplus
}
#endif

#endif
