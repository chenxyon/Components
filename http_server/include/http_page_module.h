#ifndef HTTP_PAGE_MODULE_H
#define HTTP_PAGE_MODULE_H

#include "esp_http_server.h"

typedef httpd_uri_t http_page_route_t;

typedef struct {
    const char *name;
    const http_page_route_t *routes;
    int route_count;
} http_page_module_t;

#endif
