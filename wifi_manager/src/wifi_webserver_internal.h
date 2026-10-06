/**
 * @file     wifi_webserver_internal.h
 * @brief    wifi_manager 组件内部接口（不对外暴露）
 *
 * 功能：为同组件内的 captive_portal.c 提供 HTTP 服务器句柄
 * 修改：2026-10-03 新建
 *
 * @note     该头文件位于 src/ 下，不在 INCLUDE_DIRS 中，因此只有组件内部可见，
 *          不会污染使用方的命名空间。
 */
#ifndef WIFI_WEBSERVER_INTERNAL_H
#define WIFI_WEBSERVER_INTERNAL_H

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  获取配网 HTTP 服务器句柄
 *
 * 功能：供 Captive Portal 注册 302 重定向处理器
 * @return 服务器句柄；未启动时返回 NULL
 */
httpd_handle_t wifi_webserver_handle(void);

/**
 * @brief  启动配网 Web 服务器（供 wifi_manager.c 调用）
 */
esp_err_t wifi_webserver_start_internal(void);

/**
 * @brief  停止配网 Web 服务器（供 wifi_manager.c 调用）
 */
esp_err_t wifi_webserver_stop_internal(void);

/**
 * @brief  启动配置模式（AP+Web，供 wifi_manager.c 调用）
 */
esp_err_t wifi_config_mode_start_internal(int mode);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_WEBSERVER_INTERNAL_H */