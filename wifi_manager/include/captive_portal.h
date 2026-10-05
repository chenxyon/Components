/**
 * @file     captive_portal.h
 * @brief    Captive Portal（强制门户）DNS 拦截与页面重定向
 *
 * 功能：手机/电脑连上 SoftAP 后自动弹出配网页面，无需手动输入 IP
 * 修改：2026-10-03 新建
 *
 * @note     实现原理（ESP-IDF v6.1）：
 *          1. 启动 UDP 53 端口的本地 DNS 服务器，对任意域名查询
 *             统一返回 AP 的 IP（192.168.4.1）；
 *          2. HTTP 服务器注册通配处理器，对任意路径返回 302 重定向到 "/"；
 *          3. 操作系统探测连通性时（Android 的 connectivitycheck、
 *             Windows 的 msftconnecttest）被 DNS 劫持到本机，
 *             收到 302 后即弹出配网页面。
 *
 * @note     为什么不用 lwip 的 dns_server / esp_netif_tsfn：
 *          ESP-IDF v6.1 已移除 lwip/apps/dns_server.h（文件为空），
 *          esp_netif.h 中也不再提供 esp_netif_tsfn。本实现直接用原生
 *          lwIP BSD socket API，不依赖任何已移除的接口。
 */
#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** DNS 服务器监听的 UDP 端口 */
#define CAPTIVE_DNS_PORT         53

/**
 * @brief  启动 Captive Portal（DNS 拦截 + HTTP 302 重定向）
 *
 * 功能：由 wifi_webserver_start() 自动调用，调用方无需直接使用
 * 修改：2026-10-03 新建
 *
 * @param ap_ip AP 网关地址（点分十进制字符串），通常是 "192.168.4.1"
 * @return ESP_OK 成功
 */
esp_err_t captive_portal_start(const char *ap_ip);

/**
 * @brief  停止 Captive Portal
 *
 * 功能：关闭 DNS 服务器
 * 修改：2026-10-03 新建
 */
esp_err_t captive_portal_stop(void);

/**
 * @brief  查询 Captive Portal 是否运行中
 *
 * 功能：状态查询
 * 修改：2026-10-03 新建
 */
bool captive_portal_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* CAPTIVE_PORTAL_H */