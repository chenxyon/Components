/**
 * @file     captive_portal.c
 * @brief    Captive Portal（强制门户）DNS 拦截与页面重定向
 *
 * 功能：手机/电脑连上 SoftAP 后自动弹出配网页面，无需手动输入 IP
 * 修改：2026-10-03 新建
 * 修改：2026-10-05 修复：替换通配 URI（esp_http_server 不支持通配符，
 *         实际只匹配字面 "/\"，无法命中 /generate_204 等探针路径，导致不跳转）。
 *         改为注册已知探针路径表，每个返回 302 到 "/"。
 *
 * @note     实现原理（ESP-IDF v6.1）：
 *          1. 启动 UDP 53 端口的本地 DNS 服务器，对任意域名查询统一返回
 *             AP 网关 IP（192.168.4.1）；
 *          2. HTTP 服务器注册已知探针路径的 302 处理器；
 *          3. 操作系统联网探测（Android / iOS / Windows）被 DNS 劫持到本机，
 *             收到 302 后弹出配网页面。
 *
 * @note     为什么不用 lwip 的 dns_server / esp_netif_tsfn：
 *          ESP-IDF v6.1 已移除 lwip/apps/dns_server.h（文件为空），
 *          esp_netif.h 中也不再提供 esp_netif_tsfn。本实现直接用原生
 *          lwIP BSD socket API，不依赖任何已移除的接口。
 */
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"

#include "captive_portal.h"
#include "wifi_webserver_internal.h"

static const char *TAG = "captive_portal";

/* 状态 */
static volatile bool s_running = false;
static int           s_dns_fd  = -1;
static esp_ip4_addr_t s_ap_ip;                /*!< 解析后的 AP 网关地址 */
static char          s_ap_ip_str[16];

/* 已知探针路径表：各平台在检测到"未连接互联网"前会请求这些路径 */
static const char *const k_probe_paths[] = {
    "/generate_204",                    /* Android (标准) */
    "/generate_204.gif",                /* Android (旧版) */
    "/gwt/dfp/dfp.js",                  /* Android (较新) */
    "/connectivitycheck/platform.json", /* Android (较新) */
    "/connectivity-check/1x1.gif",      /* Windows / Edge */
    "/connectivity-check.txt",          /* Windows (legacy) */
    "/ncsi.txt",                        /* Windows (modern) */
    "/hotspot-detect.html",             /* iOS Safari */
    "/",                                /* 兜底：根路径 */
};
#define PROBE_PATH_COUNT (sizeof(k_probe_paths) / sizeof(k_probe_paths[0]))

/**
 * @brief  DNS 查询应答任务
 *
 * 功能：循环接收 UDP 查询包，构造应答把域名指向 AP 网关
 * 修改：2026-10-03 新建
 */
static void dns_task(void *arg)
{
    uint8_t rx[512];
    uint8_t tx[512];

    while (s_running) {
        struct sockaddr_in src;
        socklen_t src_len = sizeof(src);

        int n = recvfrom(s_dns_fd, rx, sizeof(rx), 0,
                         (struct sockaddr *)&src, &src_len);
        if (n < 12) {
            /* 不是合法 DNS 报文，或收到 ICMP 之类的杂包 */
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                /* 非阻塞模式下无数据时短暂休眠，避免忙等导致 Task Watchdog 超时 */
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            continue;
        }

        /* 只应答标准查询（A标志置位且QDCOUNT=1），忽略其它报文 */
        int qdcount = (rx[4] << 8) | rx[5];
        if (qdcount != 1 || (rx[2] & 0x80) != 0) {
            continue;   /* 非查询 或 已是应答 */
        }

        /* 找到查询区结尾（QNAME + QTYPE + QCLASS），后面紧跟是应答区 */
        int pos = 12;
        while (pos < n) {
            if ((rx[pos] & 0xC0) == 0xC0) {  /* 压缩指针，占 2 字节 */
                pos += 2;
                break;
            }
            if (rx[pos] == 0) {               /* 标签结束符 */
                pos += 1;
                break;
            }
            pos += 1 + rx[pos];               /* 跳过标签长度+内容 */
        }
        pos += 4;                             /* QTYPE(2) + QCLASS(2) */

        int qend = pos;
        int need = qend + 16;
        if (qend < 0 || need > (int)sizeof(tx)) {
            continue;
        }

        /* 应答 = 原查询 + 标志置 QR|AA|RD|RA + 1 条记录 */
        memcpy(tx, rx, qend);
        tx[2] = 0x84;   /* QR=1 RA=1 */
        tx[3] = 0x00;   /* AA=0 RD=0 RA=1 */
        tx[6] = 0x00;   /* ANCOUNT 高字节 */
        tx[7] = 0x01;   /* ANCOUNT 低字节 = 1 条应答 */
        tx[8] = 0x00;
        tx[9] = 0x00;   /* NSCOUNT = 0 */
        tx[10] = 0x00;
        tx[11] = 0x00;  /* ARCOUNT = 0 */

        /* 指针指向原查询的 QNAME */
        tx[qend + 0] = 0xC0;
        tx[qend + 1] = 0x0C;
        /* TYPE = A (0x0001)，CLASS = IN (0x0001) */
        tx[qend + 2] = 0x00;  tx[qend + 3]  = 0x01;
        tx[qend + 4] = 0x00;  tx[qend + 5]  = 0x01;
        /* TTL = 60 秒，够短以便离开热点后缓存失效 */
        tx[qend + 6]  = 0x00; tx[qend + 7]  = 0x00;
        tx[qend + 8]  = 0x00; tx[qend + 9]  = 0x3C;
        /* RDLENGTH = 4 */
        tx[qend + 10] = 0x00; tx[qend + 11] = 0x04;
        /* RDATA = AP 网关 IP */
        memcpy(&tx[qend + 12], &s_ap_ip.addr, 4);

        sendto(s_dns_fd, tx, need, 0, (struct sockaddr *)&src, src_len);
    }

    s_dns_fd = -1;
    vTaskDelete(NULL);
}

/**
 * @brief  HTTP 302 重定向处理器
 *
 * 功能：被所有探针路径注册后调用，把请求重定向到配网首页
 * 修改：2026-10-05 改为通用处理器（不再依赖通配 URI）
 *
 * @param req HTTP 请求句柄
 */
static esp_err_t redirect_handler(httpd_req_t *req)
{
    char url[128];
    snprintf(url, sizeof(url), "http://%s/", s_ap_ip_str);

    /* 明确告诉客户端这是临时跳转，避免浏览器缓存 */
    httpd_resp_set_hdr(req, "Location", url);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_sendstr(req,
        "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
        "<meta http-equiv=\"refresh\" content=\"0;url=/\">"
        "<title>正在跳转</title></head><body>"
        "Redirecting to WiFi setup page...</body></html>");

    return ESP_OK;
}

/**
 * @brief  启动 Captive Portal
 *
 * 功能：注册 HTTP 302 探针路径处理器 + 启动本地 DNS 服务器
 * 修改：2026-10-03 新建
 * 修改：2026-10-05 修复：改为注册已知探针路径表，替代无效的通配 URI
 *
 * @param ap_ip AP 网关地址字符串，通常是 "192.168.4.1"
 */
esp_err_t captive_portal_start(const char *ap_ip)
{
    if (s_running) {
        return ESP_OK;
    }
    if (ap_ip == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 1. 注册已知探针路径的 302 处理器 */
    httpd_handle_t server = wifi_webserver_handle();
    if (server != NULL) {
        for (int i = 0; i < PROBE_PATH_COUNT; i++) {
            httpd_uri_t uri = {
                .uri      = k_probe_paths[i],
                .method   = HTTP_GET,
                .handler  = redirect_handler,
                .user_ctx = NULL,
            };
            esp_err_t err = httpd_register_uri_handler(server, &uri);
            if (err == ESP_ERR_HTTPD_HANDLER_EXISTS) {
                /* 重复注册（例如 root_handler 已经占用了 "/"）*/
                continue;
            }
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "注册 302 处理器 %s 失败：%s",
                         k_probe_paths[i], esp_err_to_name(err));
            } else {
                ESP_LOGI(TAG, "注册探针路径：%s", k_probe_paths[i]);
            }
        }
    }

    /* 2. 启动 DNS 服务器 */
    esp_ip4_addr_t ip;
    if (esp_netif_str_to_ip4(ap_ip, &ip) != ESP_OK) {
        ESP_LOGE(TAG, "AP 地址解析失败：%s", ap_ip);
        return ESP_ERR_INVALID_ARG;
    }
    s_ap_ip = ip;
    strlcpy(s_ap_ip_str, ap_ip, sizeof(s_ap_ip_str));

    s_dns_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_dns_fd < 0) {
        ESP_LOGE(TAG, "创建 DNS socket 失败：errno=%d", errno);
        return ESP_FAIL;
    }

    /* 非阻塞，避免任务空转烧 CPU */
    int flags = fcntl(s_dns_fd, F_GETFL, 0);
    fcntl(s_dns_fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in local = {
        .sin_family      = AF_INET,
        .sin_port        = htons(CAPTIVE_DNS_PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };
    if (bind(s_dns_fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        ESP_LOGE(TAG, "绑定 UDP 53 失败：errno=%d", errno);
        close(s_dns_fd);
        s_dns_fd = -1;
        return ESP_FAIL;
    }

    s_running = true;
    BaseType_t ok = xTaskCreate(dns_task, "capdns", 4096, NULL, 4, NULL);
    if (ok != pdPASS) {
        s_running = false;
        close(s_dns_fd);
        s_dns_fd = -1;
        ESP_LOGE(TAG, "DNS 任务创建失败");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Captive Portal 已启动：DNS -> %s，探针路径 -> 302 到首页", ap_ip);
    return ESP_OK;
}

/**
 * @brief  停止 Captive Portal
 *
 * 功能：关闭 socket 使 DNS 任务退出循环
 * 修改：2026-10-03 新建
 */
esp_err_t captive_portal_stop(void)
{
    if (!s_running) {
        return ESP_OK;
    }

    s_running = false;

    /* 关闭 socket 让 DNS 任务从 recvfrom 退出，然后等待任务自行结束 */
    if (s_dns_fd >= 0) {
        shutdown(s_dns_fd, SHUT_RDWR);
        close(s_dns_fd);
        s_dns_fd = -1;
    }

    /* 最多等 500ms 让 DNS 任务退出 */
    for (int i = 0; i < 50; i++) {
        if (s_dns_fd < 0) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "Captive Portal 已停止");
    return ESP_OK;
}

bool captive_portal_is_running(void)
{
    return s_running;
}
