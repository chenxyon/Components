#ifndef FONT_PARTITION_CHECK_H
#define FONT_PARTITION_CHECK_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_FONT_PARTITION_REQUIRED
#if CONFIG_FONT_PARTITION_SIZE_MIN < 2048
#error "Font partition component: Minimum partition size must be at least 2048KB (2MB) for GBK font data"
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
