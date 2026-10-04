#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>

#define SETTINGS_VERSION "1.0.0"

typedef struct {
    char project_version[32];
    char component_version[32];
    char ip_address[16];
    char ssid[32];
    int wifi_signal_level;
    bool wifi_connected;
    bool bluetooth_enabled;
    char bluetooth_device_name[32];
    int volume;
    int brightness;
    int key_sensitivity;
} settings_data_t;

void settings_ui_init(void);
void settings_ui_deinit(void);
void settings_ui_show(void);
void settings_ui_hide(void);

void settings_ui_set_version(const char *project_version, const char *component_version);
void settings_ui_update_wifi_status(const char *ssid, const char *ip_address, int signal_level, bool connected);
void settings_ui_update_bluetooth_status(bool enabled, const char *device_name);

int settings_ui_get_volume(void);
int settings_ui_get_brightness(void);
int settings_ui_get_key_sensitivity(void);

void settings_ui_set_volume(int volume);
void settings_ui_set_brightness(int brightness);
void settings_ui_set_key_sensitivity(int sensitivity);

void settings_ui_reset_to_defaults(void);

#ifdef __cplusplus
}
#endif

#endif
