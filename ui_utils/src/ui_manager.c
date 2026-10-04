#include "ui_manager.h"
#include <stdlib.h>
#include <stdio.h>

static app_desc_t g_app_list[MAX_APP_COUNT];
static int g_app_count = 0;
static lv_obj_t *g_desktop = NULL;
static lv_group_t *g_group = NULL;
static lv_obj_t *g_current_page_obj = NULL;
static app_id_t g_current_launching_app_id = 0;
static desktop_layout_t g_layout = DESKTOP_LAYOUT_4x3;
static int g_current_page = 0;
static lv_obj_t *g_page_indicator = NULL;

static const desktop_layout_info_t g_layout_info[DESKTOP_LAYOUT_MAX] = {
    {2, 2, 180, 200, 120},  
    {2, 3, 180, 140, 100},  
    {3, 3, 120, 140, 80},   
    {4, 2, 100, 200, 70},   
    {4, 3, 100, 140, 65},   
};

static void app_icon_click_cb(lv_event_t *e);
static void update_desktop_icons(void);
static void update_page_indicator(void);
static int get_visible_app_count(void);

void ui_manager_init(void) {
    g_desktop = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_desktop, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(g_desktop, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(g_desktop, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(g_desktop, 20, 0);
    lv_obj_set_style_pad_hor(g_desktop, 30, 0);
    lv_obj_set_style_pad_row(g_desktop, 20, 0);
    
    g_group = lv_group_create();
    
    g_page_indicator = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(g_page_indicator, &lv_font_montserrat_14, 0);
    lv_obj_align(g_page_indicator, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    update_desktop_icons();
}

void ui_manager_register_app(const app_desc_t *app) {
    if (app == NULL || g_app_count >= MAX_APP_COUNT) {
        return;
    }
    
    for (int i = 0; i < g_app_count; i++) {
        if (g_app_list[i].id == app->id) {
            return;
        }
    }
    
    g_app_list[g_app_count] = *app;
    if (g_app_list[g_app_count].visible == -1) {
        g_app_list[g_app_count].visible = 1;
    }
    g_app_count++;
    
    update_desktop_icons();
}

void ui_manager_unregister_app(app_id_t id) {
    for (int i = 0; i < g_app_count; i++) {
        if (g_app_list[i].id == id) {
            for (int j = i; j < g_app_count - 1; j++) {
                g_app_list[j] = g_app_list[j + 1];
            }
            g_app_count--;
            
            if (g_current_page >= ui_manager_get_total_pages()) {
                g_current_page = LV_MAX(0, ui_manager_get_total_pages() - 1);
            }
            
            update_desktop_icons();
            return;
        }
    }
}

static int get_visible_app_count(void) {
    int count = 0;
    for (int i = 0; i < g_app_count; i++) {
        if (g_app_list[i].visible == 1) {
            count++;
        }
    }
    return count;
}

int ui_manager_get_total_pages(void) {
    const desktop_layout_info_t *info = &g_layout_info[g_layout];
    int apps_per_page = info->cols * info->rows;
    int visible_count = get_visible_app_count();
    
    if (visible_count <= 0) {
        return 0;
    }
    
    return (visible_count + apps_per_page - 1) / apps_per_page;
}

int ui_manager_get_current_page(void) {
    return g_current_page;
}

void ui_manager_set_page(int page) {
    int total_pages = ui_manager_get_total_pages();
    if (total_pages <= 0) {
        g_current_page = 0;
        return;
    }
    
    if (page < 0) {
        g_current_page = 0;
    } else if (page >= total_pages) {
        g_current_page = total_pages - 1;
    } else {
        g_current_page = page;
    }
    
    update_desktop_icons();
}

void ui_manager_next_page(void) {
    int total_pages = ui_manager_get_total_pages();
    if (total_pages > 1 && g_current_page < total_pages - 1) {
        g_current_page++;
        update_desktop_icons();
        ui_manager_notify_event(UI_EVENT_PAGE_CHANGED);
    }
}

void ui_manager_prev_page(void) {
    if (g_current_page > 0) {
        g_current_page--;
        update_desktop_icons();
        ui_manager_notify_event(UI_EVENT_PAGE_CHANGED);
    }
}

desktop_layout_t ui_manager_get_layout(void) {
    return g_layout;
}

void ui_manager_set_layout(desktop_layout_t layout) {
    if (layout < 0 || layout >= DESKTOP_LAYOUT_MAX) {
        return;
    }
    
    g_layout = layout;
    g_current_page = 0;
    update_desktop_icons();
}

static void update_page_indicator(void) {
    if (g_page_indicator == NULL) {
        return;
    }
    
    int total_pages = ui_manager_get_total_pages();
    if (total_pages <= 1) {
        lv_label_set_text(g_page_indicator, "");
        return;
    }
    
    char buf[32];
    snprintf(buf, sizeof(buf), "%d/%d", g_current_page + 1, total_pages);
    lv_label_set_text(g_page_indicator, buf);
}

static void update_desktop_icons(void) {
    if (g_desktop == NULL) {
        return;
    }
    
    uint32_t child_count = lv_obj_get_child_cnt(g_desktop);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(g_desktop, i);
        void *user_data = lv_obj_get_user_data(child);
        if (user_data != NULL) {
            free(user_data);
        }
    }
    
    lv_obj_clean(g_desktop);
    
    const desktop_layout_info_t *info = &g_layout_info[g_layout];
    int apps_per_page = info->cols * info->rows;
    int start_idx = g_current_page * apps_per_page;
    
    int visible_idx = 0;
    for (int i = 0; i < g_app_count; i++) {
        if (g_app_list[i].visible != 1) {
            continue;
        }
        
        if (visible_idx >= start_idx + apps_per_page) {
            break;
        }
        
        if (visible_idx >= start_idx) {
            lv_obj_t *btn = lv_btn_create(g_desktop);
            lv_obj_set_size(btn, info->btn_width, info->btn_height);
            lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            
            if (g_app_list[i].icon_type == ICON_TYPE_IMAGE && g_app_list[i].icon != NULL) {
                lv_obj_t *img = lv_img_create(btn);
                lv_img_set_src(img, g_app_list[i].icon);
                lv_obj_set_size(img, info->icon_size, info->icon_size);
                lv_obj_center(img);
            } else if (g_app_list[i].icon_type == ICON_TYPE_SYMBOL && g_app_list[i].icon != NULL) {
                lv_obj_t *symbol_label = lv_label_create(btn);
                lv_label_set_text(symbol_label, (const char *)g_app_list[i].icon);
                
                int font_size = info->icon_size / 2;
                if (font_size < 12) font_size = 12;
                if (font_size > 48) font_size = 48;
                
                const lv_font_t *font = &lv_font_montserrat_14;
                
                lv_obj_set_style_text_font(symbol_label, font, 0);
                lv_obj_set_size(symbol_label, info->icon_size, info->icon_size);
                lv_obj_center(symbol_label);
            } else {
                lv_obj_t *icon_container = lv_obj_create(btn);
                lv_obj_set_size(icon_container, info->icon_size, info->icon_size);
            }
            
            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, g_app_list[i].name);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
            
            app_id_t *id_ptr = (app_id_t *)malloc(sizeof(app_id_t));
            *id_ptr = g_app_list[i].id;
            lv_obj_set_user_data(btn, id_ptr);
            
            lv_obj_add_event_cb(btn, app_icon_click_cb, LV_EVENT_CLICKED, NULL);
            
            lv_group_add_obj(g_group, btn);
        }
        
        visible_idx++;
    }
    
    update_page_indicator();
}

static void app_icon_click_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    app_id_t *id_ptr = (app_id_t *)lv_obj_get_user_data(btn);
    if (id_ptr != NULL) {
        ui_manager_launch_app(*id_ptr);
    }
}

void ui_manager_launch_app(app_id_t id) {
    for (int i = 0; i < g_app_count; i++) {
        if (g_app_list[i].id == id && g_app_list[i].launch_cb != NULL) {
            g_current_launching_app_id = id;
            lv_obj_clean(lv_scr_act());
            g_current_page_obj = lv_obj_create(lv_scr_act());
            lv_obj_set_size(g_current_page_obj, LV_HOR_RES, LV_VER_RES);
            g_app_list[i].launch_cb();
            return;
        }
    }
}

void ui_manager_return_to_desktop(void) {
    lv_obj_clean(lv_scr_act());
    ui_manager_init();
}

void ui_manager_notify_event(ui_event_t event) {
    (void)event;
}

bool ui_manager_is_app_registered(app_id_t id) {
    for (int i = 0; i < g_app_count; i++) {
        if (g_app_list[i].id == id) {
            return true;
        }
    }
    return false;
}

bool ui_manager_get_app_visible(app_id_t id) {
    for (int i = 0; i < g_app_count; i++) {
        if (g_app_list[i].id == id) {
            return (g_app_list[i].visible == 1);
        }
    }
    return false;
}

void ui_manager_set_app_visible(app_id_t id, bool visible) {
    for (int i = 0; i < g_app_count; i++) {
        if (g_app_list[i].id == id) {
            g_app_list[i].visible = visible ? 1 : 0;
            
            if (!visible && g_current_page >= ui_manager_get_total_pages()) {
                g_current_page = LV_MAX(0, ui_manager_get_total_pages() - 1);
            }
            
            update_desktop_icons();
            return;
        }
    }
}

app_id_t get_current_launching_app_id(void) {
    return g_current_launching_app_id;
}
