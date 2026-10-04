#include "version_check.h"
#include <string.h>

static version_info_t g_version_info = {
    .current_version = "v1.0.0",
    .latest_version = "v1.0.0",
    .update_log = "",
    .package_size = "",
    .update_available = false,
    .check_in_progress = false,
};

static int compare_versions(const char *v1, const char *v2) {
    if (v1 == NULL || v2 == NULL) {
        return 0;
    }
    
    const char *p1 = v1;
    const char *p2 = v2;
    
    if (*p1 == 'v' || *p1 == 'V') p1++;
    if (*p2 == 'v' || *p2 == 'V') p2++;
    
    while (*p1 != '\0' || *p2 != '\0') {
        int num1 = 0, num2 = 0;
        
        while (*p1 >= '0' && *p1 <= '9') {
            num1 = num1 * 10 + (*p1 - '0');
            p1++;
        }
        
        while (*p2 >= '0' && *p2 <= '9') {
            num2 = num2 * 10 + (*p2 - '0');
            p2++;
        }
        
        if (num1 > num2) return 1;
        if (num1 < num2) return -1;
        
        if (*p1 == '.') p1++;
        if (*p2 == '.') p2++;
    }
    
    return 0;
}

static void simulate_version_check(void) {
    strcpy(g_version_info.latest_version, "v1.2.0");
    strcpy(g_version_info.update_log, "- Added new features\n- Bug fixes\n- Performance improvements");
    strcpy(g_version_info.package_size, "1.5 MB");
    
    int cmp = compare_versions(g_version_info.latest_version, g_version_info.current_version);
    g_version_info.update_available = (cmp > 0);
    
    g_version_info.check_in_progress = false;
}

void version_check_init(void) {
}

void version_check_deinit(void) {
}

void version_check_start(void) {
    if (g_version_info.check_in_progress) {
        return;
    }
    
    g_version_info.check_in_progress = true;
    
    simulate_version_check();
}

void version_check_stop(void) {
    g_version_info.check_in_progress = false;
}

const version_info_t *version_check_get_info(void) {
    return &g_version_info;
}

bool version_check_has_update(void) {
    return g_version_info.update_available;
}
