#include <stddef.h>
#include "font_service.h"
#include "font_builtin.h"

#ifdef CONFIG_FONT_SERVICE_ENABLE_GBK
#include "font_gbk.h"
#endif

#ifdef CONFIG_FONT_SERVICE_ENABLE_YUESONG
#include "font_yuesong.h"
#endif

#ifdef CONFIG_FONT_SERVICE_ENABLE_W25Q
#include "font_w25q.h"
#endif

FontService *font_service_create(font_type_t type) {
    FontService *service = NULL;
    
    switch (type) {
        case FONT_TYPE_BUILTIN:
            service = font_builtin_create();
            break;
#ifdef CONFIG_FONT_SERVICE_ENABLE_GBK
        case FONT_TYPE_GBK:
            service = font_gbk_create();
            break;
#endif
#ifdef CONFIG_FONT_SERVICE_ENABLE_YUESONG
        case FONT_TYPE_YUESONG:
            service = font_yuesong_create();
            break;
#endif
#ifdef CONFIG_FONT_SERVICE_ENABLE_W25Q
        case FONT_TYPE_W25Q:
            service = font_w25q_create();
            break;
#endif
        default:
            break;
    }
    
    return service;
}
