/**
 * @file page_wifi_config.c
 * @brief WiFi配置页面模块
 *
 * 内置页面模块：提供WiFi扫描、配置保存、STA连接功能
 * 通过 http_page_module_t 接口注册到 http_server
 */

#include "http_page_module.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "WIFI_CONFIG";

/* ==================== WiFi连接任务 ==================== */

static void wifi_connect_task(void *arg) {
    char *ssid = (char *)arg;
    char *password = ssid + 33;

    vTaskDelay(pdMS_TO_TICKS(500));

    /* 保持APSTA模式，AP热点不中断 */
    esp_wifi_set_mode(WIFI_MODE_APSTA);

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, password, sizeof(sta_cfg.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);

    for (int retry = 0; retry < 3; retry++) {
        esp_err_t err = esp_wifi_connect();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Connecting to WiFi: %s (attempt %d)", ssid, retry + 1);
            vTaskDelay(pdMS_TO_TICKS(5000));
            break;
        }
        ESP_LOGW(TAG, "esp_wifi_connect failed: %s (attempt %d)", esp_err_to_name(err), retry + 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    free(ssid);
    vTaskDelete(NULL);
}

/* ==================== HTML页面 ==================== */

static const char *wifi_config_html =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>GuzhengTuner - WiFi Setup</title>"
"<style>"
"body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f2f5;}"
".card{max-width:420px;margin:40px auto;background:#fff;border-radius:12px;box-shadow:0 4px 20px rgba(0,0,0,0.1);padding:30px;}"
"h1{text-align:center;color:#333;font-size:24px;margin-bottom:8px;}"
".subtitle{text-align:center;color:#888;font-size:14px;margin-bottom:24px;}"
".form-group{margin-bottom:18px;}"
"label{display:block;margin-bottom:6px;color:#555;font-weight:bold;font-size:14px;}"
"input[type=text],input[type=password]{width:100%;padding:12px;border:1px solid #ddd;border-radius:8px;box-sizing:border-box;font-size:15px;}"
"input:focus{border-color:#4CAF50;outline:none;box-shadow:0 0 0 2px rgba(76,175,80,0.2);}"
"button{width:100%;padding:13px;border:none;border-radius:8px;font-size:16px;font-weight:bold;cursor:pointer;margin-bottom:10px;}"
".btn-save{background:#4CAF50;color:#fff;}"
".btn-save:hover{background:#43a047;}"
".btn-scan{background:#2196F3;color:#fff;}"
".btn-scan:hover{background:#1e88e5;}"
".wifi-list{margin-bottom:18px;max-height:200px;overflow-y:auto;border:1px solid #eee;border-radius:8px;}"
".wifi-item{padding:10px 14px;border-bottom:1px solid #f0f0f0;cursor:pointer;display:flex;justify-content:space-between;align-items:center;}"
".wifi-item:last-child{border-bottom:none;}"
".wifi-item:hover{background:#e8f5e9;}"
".wifi-item.selected{background:#c8e6c9;}"
".wifi-name{font-weight:bold;color:#333;}"
".wifi-rssi{font-size:12px;color:#888;}"
".status{margin-top:12px;padding:10px;border-radius:8px;text-align:center;display:none;font-size:14px;}"
".status.ok{background:#d4edda;color:#155724;display:block;}"
".status.err{background:#f8d7da;color:#721c24;display:block;}"
".scanning{text-align:center;color:#888;padding:10px;}"
"</style></head><body>"
"<div class='card'>"
"<h1>GuzhengTuner</h1>"
"<p class='subtitle'>WiFi Configuration</p>"
"<div id='wifi-list' class='wifi-list' style='display:none'></div>"
"<form id='wifiForm'>"
"<div class='form-group'>"
"<label>WiFi Name (SSID)</label>"
"<input type='text' id='ssid' placeholder='Enter WiFi name' required>"
"</div>"
"<div class='form-group'>"
"<label>Password</label>"
"<input type='password' id='password' placeholder='Enter password'>"
"</div>"
"<button type='submit' class='btn-save'>Save & Connect</button>"
"</form>"
"<button class='btn-scan' onclick='scanWifi()'>Scan WiFi</button>"
"<div id='scanning' class='scanning' style='display:none'>Scanning...</div>"
"<div id='status' class='status'></div>"
"</div>"
"<script>"
"var pollTimer=null;"
"function scanWifi(){"
"var el=document.getElementById('wifi-list');"
"el.innerHTML='';el.style.display='block';"
"document.getElementById('scanning').style.display='block';"
"fetch('/api/scan').then(function(r){return r.json();}).then(function(d){"
"if(d.scanning){pollTimer=setTimeout(scanWifi,800);return;}"
"document.getElementById('scanning').style.display='none';"
"if(pollTimer){clearTimeout(pollTimer);pollTimer=null;}"
"if(d.aps&&d.aps.length>0){"
"var h='';"
"d.aps.forEach(function(a){"
"h+='<div class=\"wifi-item\" onclick=\"pick(\\''+a.ssid+'\\')\">';"
"h+='<span class=\"wifi-name\">'+a.ssid+'</span>';"
"h+='<span class=\"wifi-rssi\">'+a.rssi+' dBm</span></div>';"
"});el.innerHTML=h;"
"}else{el.innerHTML='<div style=\"padding:10px;text-align:center;color:#999\">No WiFi found</div>';}"
"}).catch(function(){document.getElementById('scanning').style.display='none';});"
"}"
"function pick(s){document.getElementById('ssid').value=s;"
"document.querySelectorAll('.wifi-item').forEach(function(e){"
"e.classList.toggle('selected',e.querySelector('.wifi-name').textContent===s);});}"
"document.getElementById('wifiForm').addEventListener('submit',function(e){"
"e.preventDefault();"
"var s=document.getElementById('ssid').value;"
"var p=document.getElementById('password').value;"
"fetch('/config',{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({ssid:s,password:p})})"
".then(function(r){return r.json();}).then(function(d){"
"var st=document.getElementById('status');"
"if(d.success){st.className='status ok';st.textContent='Saved! Connecting...';}"
"else{st.className='status err';st.textContent='Failed: '+(d.error||'unknown');}"
"}).catch(function(){"
"var st=document.getElementById('status');st.className='status err';st.textContent='Request failed';});"
"});"
"</script></body></html>";

/* ==================== 路由处理函数 ==================== */

static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, wifi_config_html, strlen(wifi_config_html));
}

/* WiFi扫描状态 */
static bool s_scan_started = false;
static TickType_t s_scan_start_tick = 0;

static esp_err_t scan_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    if (s_scan_started) {
        TickType_t elapsed = xTaskGetTickCount() - s_scan_start_tick;
        if (elapsed < pdMS_TO_TICKS(2000)) {
            httpd_resp_send(req, "{\"scanning\":true}", 17);
            return ESP_OK;
        }

        wifi_ap_record_t ap_list[20];
        uint16_t ap_count = 20;
        esp_err_t err = esp_wifi_scan_get_ap_records(&ap_count, ap_list);
        if (err != ESP_OK) {
            if (elapsed < pdMS_TO_TICKS(8000)) {
                httpd_resp_send(req, "{\"scanning\":true}", 17);
                return ESP_OK;
            }
            s_scan_started = false;
            httpd_resp_send(req, "{\"aps\":[]}", 11);
            return ESP_OK;
        }

        s_scan_started = false;
        if (ap_count > 20) ap_count = 20;

        char resp[2048] = "{\"aps\":[";
        for (int i = 0; i < ap_count; i++) {
            if (i > 0) strcat(resp, ",");
            char item[200];
            snprintf(item, sizeof(item), "{\"ssid\":\"%s\",\"rssi\":%d}",
                     (char *)ap_list[i].ssid, ap_list[i].rssi);
            strcat(resp, item);
        }
        strcat(resp, "]}");
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }

    esp_err_t err = esp_wifi_scan_start(NULL, false);
    if (err == ESP_OK) {
        s_scan_started = true;
        s_scan_start_tick = xTaskGetTickCount();
        httpd_resp_send(req, "{\"scanning\":true}", 17);
    } else {
        ESP_LOGW(TAG, "Scan start failed: %s", esp_err_to_name(err));
        httpd_resp_send(req, "{\"aps\":[]}", 11);
    }
    return ESP_OK;
}

static esp_err_t config_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send(req, "{\"success\":false,\"error\":\"read failed\"}", 34);
        return ESP_OK;
    }
    buf[ret] = '\0';

    char ssid[33] = {0};
    char password[65] = {0};

    char *p = strstr(buf, "\"ssid\"");
    if (p) { p = strchr(p, ':'); if (p) { p = strchr(p, '"'); if (p) { p++; char *e = strchr(p, '"'); if (e && (e - p) < (int)sizeof(ssid)) { strncpy(ssid, p, e - p); } } } }
    p = strstr(buf, "\"password\"");
    if (p) { p = strchr(p, ':'); if (p) { p = strchr(p, '"'); if (p) { p++; char *e = strchr(p, '"'); if (e && (e - p) < (int)sizeof(password)) { strncpy(password, p, e - p); } } } }

    if (strlen(ssid) == 0) {
        httpd_resp_send(req, "{\"success\":false,\"error\":\"SSID required\"}", 38);
        return ESP_OK;
    }

    /* 保存到NVS */
    nvs_handle_t nvs;
    if (nvs_open("wifi_config", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "ssid", ssid);
        nvs_set_str(nvs, "password", password);
        nvs_set_u8(nvs, "saved", 1);
        nvs_commit(nvs);
        nvs_close(nvs);
        ESP_LOGI(TAG, "WiFi config saved: SSID=%s", ssid);
    }

    /* 先发响应，再启动连接任务 */
    httpd_resp_send(req, "{\"success\":true}", 16);

    char *params = malloc(33 + 65);
    if (params) {
        memset(params, 0, 33 + 65);
        strncpy(params, ssid, 32);
        strncpy(params + 33, password, 64);
        xTaskCreatePinnedToCore(wifi_connect_task, "wifi_connect", 4096, params, 5, NULL, 1);
    }

    return ESP_OK;
}

/* ==================== 模块注册 ==================== */

static const http_page_route_t wifi_config_routes[] = {
    { "/",          HTTP_GET,  root_handler,   NULL },
    { "/api/scan",  HTTP_GET,  scan_handler,   NULL },
    { "/config",    HTTP_POST, config_handler,  NULL },
};

http_page_module_t page_wifi_config_get_module(void) {
    http_page_module_t mod = {
        .name = "wifi_config_page",
        .routes = wifi_config_routes,
        .route_count = 3,
    };
    return mod;
}
