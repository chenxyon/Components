#include "settings_ui.h"
#include "ui_manager.h"
#include "ui_utils.h"
#include <string.h>
#include <stdio.h>

static settings_data_t g_settings = {
    .project_version = "v1.0.0",
    .component_version = SETTINGS_VERSION,
    .ip_address = "0.0.0.0",
    .ssid = "Not connected",
    .wifi_signal_level = 0,
    .wifi_connected = false,
    .bluetooth_enabled = false,
    .bluetooth_device_name = "Unknown",
    .volume = 50,
    .brightness = 80,
    .key_sensitivity = 50,
};

static lv_obj_t *g_settings_page = NULL;
static lv_obj_t *g_current_subpage = NULL;

static void back_btn_cb(lv_event_t *e) {
    if (g_current_subpage != NULL) {
        lv_obj_del(g_current_subpage);
        g_current_subpage = NULL;
    } else {
        ui_manager_return_to_desktop();
    }
}

static void create_back_btn(lv_obj_t *parent) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 60, 40);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 10, 10);
    
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Back");
    lv_obj_center(label);
    
    lv_obj_add_event_cb(btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
}

static void create_system_info_page(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_HOR_RES - 40, LV_VER_RES - 100);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 20, 0);
    
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "System Info");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    
    lv_obj_t *project_ver = lv_label_create(cont);
    char project_buf[64];
    snprintf(project_buf, sizeof(project_buf), "Project Version: %s", g_settings.project_version);
    lv_label_set_text(project_ver, project_buf);
    
    lv_obj_t *component_ver = lv_label_create(cont);
    char component_buf[64];
    snprintf(component_buf, sizeof(component_buf), "Component Version: %s", g_settings.component_version);
    lv_label_set_text(component_ver, component_buf);
    
    g_current_subpage = cont;
}

static void create_network_page(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_HOR_RES - 40, LV_VER_RES - 100);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 15, 0);
    
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Network");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    
    lv_obj_t *ssid_label = lv_label_create(cont);
    char ssid_buf[64];
    snprintf(ssid_buf, sizeof(ssid_buf), "SSID: %s", g_settings.ssid);
    lv_label_set_text(ssid_label, ssid_buf);
    
    lv_obj_t *ip_label = lv_label_create(cont);
    char ip_buf[64];
    snprintf(ip_buf, sizeof(ip_buf), "IP Address: %s", g_settings.ip_address);
    lv_label_set_text(ip_label, ip_buf);
    
    lv_obj_t *conn_label = lv_label_create(cont);
    lv_label_set_text(conn_label, g_settings.wifi_connected ? "Connected" : "Disconnected");
    lv_obj_set_style_text_color(conn_label, g_settings.wifi_connected ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000), 0);
    
    lv_obj_t *scan_btn = lv_btn_create(cont);
    lv_obj_set_size(scan_btn, 150, 40);
    lv_obj_t *scan_label = lv_label_create(scan_btn);
    lv_label_set_text(scan_label, "Scan WiFi");
    lv_obj_center(scan_label);
    
    lv_obj_t *hotspot_btn = lv_btn_create(cont);
    lv_obj_set_size(hotspot_btn, 150, 40);
    lv_obj_t *hotspot_label = lv_label_create(hotspot_btn);
    lv_label_set_text(hotspot_label, "Enable Hotspot");
    lv_obj_center(hotspot_label);
    
    g_current_subpage = cont;
}

static void create_bluetooth_page(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_HOR_RES - 40, LV_VER_RES - 100);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 15, 0);
    
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Bluetooth");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    
    lv_obj_t *status_label = lv_label_create(cont);
    lv_label_set_text(status_label, g_settings.bluetooth_enabled ? "Enabled" : "Disabled");
    lv_obj_set_style_text_color(status_label, g_settings.bluetooth_enabled ? lv_color_hex(0x00FF00) : lv_color_hex(0xFF0000), 0);
    
    lv_obj_t *device_label = lv_label_create(cont);
    char device_buf[64];
    snprintf(device_buf, sizeof(device_buf), "Device Name: %s", g_settings.bluetooth_device_name);
    lv_label_set_text(device_label, device_buf);
    
    lv_obj_t *toggle_btn = lv_btn_create(cont);
    lv_obj_set_size(toggle_btn, 150, 40);
    lv_obj_t *toggle_label = lv_label_create(toggle_btn);
    lv_label_set_text(toggle_label, g_settings.bluetooth_enabled ? "Disable" : "Enable");
    lv_obj_center(toggle_label);
    
    lv_obj_t *search_btn = lv_btn_create(cont);
    lv_obj_set_size(search_btn, 150, 40);
    lv_obj_t *search_label = lv_label_create(search_btn);
    lv_label_set_text(search_label, "Search Devices");
    lv_obj_center(search_label);
    
    lv_obj_t *send_btn = lv_btn_create(cont);
    lv_obj_set_size(send_btn, 150, 40);
    lv_obj_t *send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, "Send File");
    lv_obj_center(send_label);
    
    g_current_subpage = cont;
}

static void volume_slider_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    g_settings.volume = lv_slider_get_value(slider);
}

static void create_sound_page(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_HOR_RES - 40, LV_VER_RES - 100);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 20, 0);
    
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Sound");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    
    lv_obj_t *volume_label = lv_label_create(cont);
    char volume_buf[32];
    snprintf(volume_buf, sizeof(volume_buf), "Volume: %d%%", g_settings.volume);
    lv_label_set_text(volume_label, volume_buf);
    
    lv_obj_t *slider = lv_slider_create(cont);
    lv_obj_set_size(slider, 200, 20);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, g_settings.volume, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, volume_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t *mute_btn = lv_btn_create(cont);
    lv_obj_set_size(mute_btn, 100, 40);
    lv_obj_t *mute_label = lv_label_create(mute_btn);
    lv_label_set_text(mute_label, "Mute");
    lv_obj_center(mute_label);
    
    g_current_subpage = cont;
}

static void brightness_slider_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    g_settings.brightness = lv_slider_get_value(slider);
}

static void create_display_page(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_HOR_RES - 40, LV_VER_RES - 100);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 20, 0);
    
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Display");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    
    lv_obj_t *brightness_label = lv_label_create(cont);
    char brightness_buf[32];
    snprintf(brightness_buf, sizeof(brightness_buf), "Brightness: %d%%", g_settings.brightness);
    lv_label_set_text(brightness_label, brightness_buf);
    
    lv_obj_t *slider = lv_slider_create(cont);
    lv_obj_set_size(slider, 200, 20);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, g_settings.brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    g_current_subpage = cont;
}

static void sensitivity_slider_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    g_settings.key_sensitivity = lv_slider_get_value(slider);
}

static void create_keyboard_page(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_HOR_RES - 40, LV_VER_RES - 100);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 20, 0);
    
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Keyboard");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    
    lv_obj_t *sensitivity_label = lv_label_create(cont);
    char sensitivity_buf[32];
    snprintf(sensitivity_buf, sizeof(sensitivity_buf), "Sensitivity: %d", g_settings.key_sensitivity);
    lv_label_set_text(sensitivity_label, sensitivity_buf);
    
    lv_obj_t *slider = lv_slider_create(cont);
    lv_obj_set_size(slider, 200, 20);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, g_settings.key_sensitivity, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, sensitivity_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    g_current_subpage = cont;
}

static void layout_btn_click_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    desktop_layout_t *layout = (desktop_layout_t *)lv_obj_get_user_data(btn);
    if (layout != NULL) {
        ui_manager_set_layout(*layout);
    }
}

static void create_desktop_page(lv_obj_t *parent) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, LV_HOR_RES - 40, LV_VER_RES - 100);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cont, 20, 0);
    
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Desktop Layout");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    
    const char *layout_names[] = {"2x2", "2x3", "3x3", "4x2", "4x3", NULL};
    desktop_layout_t layouts[] = {DESKTOP_LAYOUT_2x2, DESKTOP_LAYOUT_2x3, DESKTOP_LAYOUT_3x3, 
                                   DESKTOP_LAYOUT_4x2, DESKTOP_LAYOUT_4x3};
    
    for (int i = 0; layout_names[i] != NULL; i++) {
        lv_obj_t *btn = lv_btn_create(cont);
        lv_obj_set_size(btn, 120, 40);
        
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, layout_names[i]);
        lv_obj_center(label);
        
        desktop_layout_t *layout_ptr = (desktop_layout_t *)malloc(sizeof(desktop_layout_t));
        *layout_ptr = layouts[i];
        lv_obj_set_user_data(btn, layout_ptr);
        
        lv_obj_add_event_cb(btn, layout_btn_click_cb, LV_EVENT_CLICKED, NULL);
        
        if (ui_manager_get_layout() == layouts[i]) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        }
    }
    
    g_current_subpage = cont;
}

static void menu_item_click_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    int *page_id = (int *)lv_obj_get_user_data(btn);
    
    if (g_current_subpage != NULL) {
        lv_obj_del(g_current_subpage);
        g_current_subpage = NULL;
    }
    
    switch (*page_id) {
        case 0:
            create_system_info_page(g_settings_page);
            break;
        case 1:
            create_network_page(g_settings_page);
            break;
        case 2:
            create_bluetooth_page(g_settings_page);
            break;
        case 3:
            create_sound_page(g_settings_page);
            break;
        case 4:
            create_display_page(g_settings_page);
            break;
        case 5:
            create_keyboard_page(g_settings_page);
            break;
        case 6:
            create_desktop_page(g_settings_page);
            break;
        case 7:
            settings_ui_reset_to_defaults();
            break;
        default:
            break;
    }
}

static void create_settings_menu(lv_obj_t *parent) {
    lv_obj_t *menu_cont = lv_obj_create(parent);
    lv_obj_set_size(menu_cont, LV_HOR_RES - 40, LV_VER_RES - 100);
    lv_obj_align(menu_cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(menu_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu_cont, LV_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(menu_cont, 10, 0);
    
    const char *menu_items[] = {
        "System Info",
        "Network",
        "Bluetooth",
        "Sound",
        "Display",
        "Keyboard",
        "Desktop",
        "Reset to Defaults",
        NULL
    };
    
    const char *menu_icons[] = {
        LV_SYMBOL_LEFT,
        LV_SYMBOL_WIFI,
        LV_SYMBOL_BLUETOOTH,
        LV_SYMBOL_AUDIO,
        LV_SYMBOL_PREV,
        LV_SYMBOL_KEYBOARD,
        LV_SYMBOL_HOME,
        LV_SYMBOL_REFRESH,
        NULL
    };
    
    for (int i = 0; menu_items[i] != NULL; i++) {
        lv_obj_t *btn = lv_btn_create(menu_cont);
        lv_obj_set_size(btn, LV_HOR_RES - 80, 50);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(btn, 15, 0);
        
        lv_obj_t *icon_label = lv_label_create(btn);
        lv_label_set_text(icon_label, menu_icons[i]);
        lv_obj_set_style_text_font(icon_label, &lv_font_montserrat_14, 0);
        
        lv_obj_t *text_label = lv_label_create(btn);
        lv_label_set_text(text_label, menu_items[i]);
        lv_obj_set_style_text_font(text_label, &lv_font_montserrat_14, 0);
        
        int *page_id = (int *)malloc(sizeof(int));
        *page_id = i;
        lv_obj_set_user_data(btn, page_id);
        
        lv_obj_add_event_cb(btn, menu_item_click_cb, LV_EVENT_CLICKED, NULL);
    }
    
    g_current_subpage = menu_cont;
}

void settings_ui_init(void) {
}

void settings_ui_deinit(void) {
    if (g_settings_page != NULL) {
        lv_obj_del(g_settings_page);
        g_settings_page = NULL;
    }
}

void settings_ui_show(void) {
    g_settings_page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_settings_page, LV_HOR_RES, LV_VER_RES);
    
    create_back_btn(g_settings_page);
    
    lv_obj_t *title = lv_label_create(g_settings_page);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    create_settings_menu(g_settings_page);
}

void settings_ui_hide(void) {
    settings_ui_deinit();
}

void settings_ui_set_version(const char *project_version, const char *component_version) {
    if (project_version != NULL) {
        strncpy(g_settings.project_version, project_version, sizeof(g_settings.project_version) - 1);
    }
    if (component_version != NULL) {
        strncpy(g_settings.component_version, component_version, sizeof(g_settings.component_version) - 1);
    }
}

void settings_ui_update_wifi_status(const char *ssid, const char *ip_address, int signal_level, bool connected) {
    if (ssid != NULL) {
        strncpy(g_settings.ssid, ssid, sizeof(g_settings.ssid) - 1);
    }
    if (ip_address != NULL) {
        strncpy(g_settings.ip_address, ip_address, sizeof(g_settings.ip_address) - 1);
    }
    g_settings.wifi_signal_level = signal_level;
    g_settings.wifi_connected = connected;
}

void settings_ui_update_bluetooth_status(bool enabled, const char *device_name) {
    g_settings.bluetooth_enabled = enabled;
    if (device_name != NULL) {
        strncpy(g_settings.bluetooth_device_name, device_name, sizeof(g_settings.bluetooth_device_name) - 1);
    }
}

int settings_ui_get_volume(void) {
    return g_settings.volume;
}

int settings_ui_get_brightness(void) {
    return g_settings.brightness;
}

int settings_ui_get_key_sensitivity(void) {
    return g_settings.key_sensitivity;
}

void settings_ui_set_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    g_settings.volume = volume;
}

void settings_ui_set_brightness(int brightness) {
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;
    g_settings.brightness = brightness;
}

void settings_ui_set_key_sensitivity(int sensitivity) {
    if (sensitivity < 0) sensitivity = 0;
    if (sensitivity > 100) sensitivity = 100;
    g_settings.key_sensitivity = sensitivity;
}

void settings_ui_reset_to_defaults(void) {
    strcpy(g_settings.project_version, "v1.0.0");
    strcpy(g_settings.component_version, SETTINGS_VERSION);
    strcpy(g_settings.ip_address, "0.0.0.0");
    strcpy(g_settings.ssid, "Not connected");
    g_settings.wifi_signal_level = 0;
    g_settings.wifi_connected = false;
    g_settings.bluetooth_enabled = false;
    strcpy(g_settings.bluetooth_device_name, "Unknown");
    g_settings.volume = 50;
    g_settings.brightness = 80;
    g_settings.key_sensitivity = 50;
    
    if (g_settings_page != NULL) {
        lv_obj_clean(g_settings_page);
        create_back_btn(g_settings_page);
        
        lv_obj_t *title = lv_label_create(g_settings_page);
        lv_label_set_text(title, "Settings");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
        
        create_settings_menu(g_settings_page);
    }
}
