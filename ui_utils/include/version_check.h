#ifndef VERSION_CHECK_H
#define VERSION_CHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>

#define MAX_VERSION_LEN 32
#define MAX_UPDATE_LOG_LEN 512
#define MAX_PACKAGE_SIZE_LEN 32

typedef struct {
    char current_version[MAX_VERSION_LEN];
    char latest_version[MAX_VERSION_LEN];
    char update_log[MAX_UPDATE_LOG_LEN];
    char package_size[MAX_PACKAGE_SIZE_LEN];
    bool update_available;
    bool check_in_progress;
} version_info_t;

void version_check_init(void);
void version_check_deinit(void);
void version_check_start(void);
void version_check_stop(void);
const version_info_t *version_check_get_info(void);
bool version_check_has_update(void);

#ifdef __cplusplus
}
#endif

#endif
