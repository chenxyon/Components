#include "http_page_module.h"
#include "app_log.h"
#include <string.h>

static esp_err_t log_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    const char *log = app_log_get_buffer();
    if (log && strlen(log) > 0) {
        return httpd_resp_send(req, log, strlen(log));
    }
    return httpd_resp_send(req, "No logs yet", 11);
}

static esp_err_t log_save_handler(httpd_req_t *req) {
    app_log_save_to_nvs();
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Log saved to NVS", 16);
}

static esp_err_t log_history_handler(httpd_req_t *req) {
    app_log_load_from_nvs();
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    const char *log = app_log_get_buffer();
    if (log && strlen(log) > 0) {
        return httpd_resp_send(req, log, strlen(log));
    }
    return httpd_resp_send(req, "No history logs", 15);
}

static const http_page_route_t routes[] = {
    { "/log",         HTTP_GET, log_handler,         NULL },
    { "/log/save",    HTTP_GET, log_save_handler,    NULL },
    { "/log/history", HTTP_GET, log_history_handler, NULL },
};

http_page_module_t page_log_get_module(void) {
    http_page_module_t mod = {
        .name = "log_page",
        .routes = routes,
        .route_count = 3,
    };
    return mod;
}
