/**
 * @file wifi_webserver.c
 * @brief WIFI Web配置服务器模块
 *
 * 实现基于HTTP服务器的WIFI配置页面：
 * - 提供WiFiManager风格中文配置界面
 * - 处理配置表单提交（/config POST）
 * - 扫描结果显示（/api/scan GET）
 */

#include "wifi_manager.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "wifi_webserver_internal.h"
#include "captive_portal.h"

static const char *TAG = "wifi_webserver";
static httpd_handle_t s_server = NULL;

httpd_handle_t wifi_webserver_handle(void) { return s_server; }

static void wifi_connect_wrapper(void *arg) {
    (void)arg;
    wifi_manager_sta_connect();
    vTaskDelete(NULL);
}

/* WiFiManager风格中文配置页 */
static const char WEB_CONFIG_PAGE[] = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WiFi配置</title>
<style>
* { margin:0; padding:0; box-sizing:border-box; }
body { font-family: "Microsoft YaHei",sans-serif; background: linear-gradient(135deg,#667eea,#764ba2); min-height:100vh; display:flex; align-items:center; justify-content:center; }
.card { background:#fff; border-radius:12px; padding:24px; width:90%; max-width:420px; box-shadow:0 8px 32px rgba(0,0,0,.18); }
h1 { text-align:center; color:#333; margin-bottom:20px; }
.btn-scan { width:100%; padding:12px; background:#28a745; color:#fff; border:none; border-radius:8px; font-size:16px; cursor:pointer; margin-bottom:12px; }
.btn-scan:hover { background:#218838; }
.btn-save { width:100%; padding:12px; background:#007bff; color:#fff; border:none; border-radius:8px; font-size:16px; margin-bottom:10px; }
.btn-save:hover { background:#0056b3; }
.label-row { display:flex; align-items:center; justify-content:space-between; margin:10px 0 4px; }
.label-row label { margin:0; }
.pwd-wrap { position:relative; }
.pwd-wrap input { padding-right:44px; }
.pwd-eye { position:absolute; right:6px; top:50%; transform:translateY(-50%);
  background:none; border:none; padding:4px; cursor:pointer; line-height:0; }
.pwd-eye svg { width:22px; height:22px; stroke:#888; fill:none; stroke-width:2;
  stroke-linecap:round; stroke-linejoin:round; }
.pwd-eye .eye-on { display:block; }
.pwd-eye.on .eye-on { display:none; }
.pwd-eye .eye-off { display:none; }
.pwd-eye.on .eye-off { display:block; }
input[type=text],input[type=password] { width:100%; padding:10px 12px; border:1px solid #ccc; border-radius:6px; font-size:15px; box-sizing:border-box; }
.wifi-list { max-height:180px; overflow-y:auto; margin-bottom:12px; border:1px solid #eee; border-radius:6px; }
.wifi-item { padding:10px 12px; border-bottom:1px solid #eee; cursor:pointer; }
.wifi-item:last-child { border-bottom:none; }
.wifi-item:hover { background:#e9f5ff; }
.wifi-item.selected { background:#cce5ff; border-left:3px solid #007bff; padding-left:9px; }
.wifi-name { font-weight:bold; }
.wifi-info { font-size:12px; color:#888; margin-top:2px; }
.status { margin-top:12px; padding:10px; border-radius:6px; text-align:center; display:none; font-size:14px; }
.status.success { background:#d4edda; color:#155724; display:block; }
.status.error { background:#f8d7da; color:#721c24; display:block; }
.scanning { background:#fff3cd; color:#856404; padding:10px; border-radius:6px; text-align:center; display:none; margin-bottom:12px; }
</style>
</head>
<body>
<div class="card">
<h1>✶ WiFi 配置</h1>
<form id="configForm">
<div class="label-row"><label for="ssid">WiFi 名称</label></div>
<input type="text" id="ssid" name="ssid" required>
<div class="label-row"><label for="password">密码</label></div>
<div class="pwd-wrap">
  <input type="password" id="password" name="password">
  <button type="button" class="pwd-eye" id="pwdEye" onclick="togglePwd()" aria-label="显示密码">
    <svg viewBox="0 0 24 24">
      <path class="eye-on" d="M1.5 12S5 5 12 5s10.5 7 10.5 7-3.5 7-10.5 7S1.5 12 1.5 12z"/>
      <circle class="eye-on" cx="12" cy="12" r="3"/>
      <path class="eye-off" d="M3 3l18 18"/>
      <path class="eye-off" d="M10.6 5.2A9.9 9.9 0 0112 5c7 0 10.5 7 10.5 7a17.6 17.6 0 01-3.3 4.1M6.4 6.4A17.3 17.3 0 001.5 12S5 19 12 19a9.7 9.7 0 004.2-.9"/>
      <path class="eye-off" d="M9.9 9.9a3 3 0 004.2 4.2"/>
    </svg>
  </button>
</div>
<button type="submit" class="btn-save">保存并连接</button>
</form>
<button class="btn-scan" onclick="doScan()">扫描可用 WiFi</button>
<div id="status" class="status"></div>
<div id="scanning" class="scanning">正在扫描...</div>
<div id="wifi-list" class="wifi-list"></div>
</div>
<script>
var sel='';
var pwdVisible=false;
function togglePwd(){
  var el=document.getElementById('password');
  var btn=document.getElementById('pwdEye');
  pwdVisible=!pwdVisible;
  el.type=pwdVisible?'text':'password';
  btn.classList.toggle('on', pwdVisible);
}
function showMsg(msg,color){
  document.getElementById('scanning').style.display='none';
  document.getElementById('wifi-list').innerHTML='<div style="text-align:center;color:'+color+'">'+msg+'</div>';
}
function doScan(){
  document.getElementById('scanning').style.display='block';
  document.getElementById('wifi-list').innerHTML='';
  fetch('/api/scan',{cache:'no-store'}).then(function(r){return r.json();})
  .then(function(d){
    document.getElementById('scanning').style.display='none';
    if(d.success&&d.aps){
      if(d.aps.length>0){
        var h='';
        d.aps.forEach(function(a){
          h+='<div class="wifi-item" onclick="pick(\''+a.ssid+'\')">';
          h+='<div class="wifi-name">'+a.ssid+'</div>';
          h+='<div class="wifi-info">信号:'+a.rssi+' dBm | '+a.auth+' | 信道:'+a.channel+'</div>';
          h+='</div>';
        });
        document.getElementById('wifi-list').innerHTML=h;
      } else {
        showMsg('未扫描到可用的 WiFi','#888');
      }
    } else {
      showMsg('扫描失败：'+(d.error||'未知错误'),'#c00');
    }
  }).catch(function(){
    showMsg('请求失败，请刷新重试','#c00');
  });
}
function pick(ssid){
  sel=ssid;
  document.getElementById('ssid').value=ssid;
  document.querySelectorAll('.wifi-item').forEach(function(el){
    el.classList.toggle('selected', el.querySelector('.wifi-name').textContent===ssid);
  });
}
document.getElementById('configForm').addEventListener('submit',function(e){
  e.preventDefault();
  var ssid=document.getElementById('ssid').value.trim();
  var pwd=document.getElementById('password').value;
  if(!ssid){ alert('请输入WiFi名称'); return; }
  fetch('/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid,password:pwd})})
  .then(function(r){return r.json();})
  .then(function(d){
    var s=document.getElementById('status');
    if(d.success){
      s.className='status success';
      s.textContent='配置已保存，正在连接…请将手机切换到目标 WiFi';
      /* AP 已停止，手机要连新 WiFi 才能访问 ESP；
         因此不再原地轮询，而是引导用户切换网络后打开新 IP 的成功页 */
      var connIv=setInterval(function(){
        fetch('/api/status',{cache:'no-store'}).then(function(r){return r.json();})
        .then(function(st){
          if(st.connected){
            clearInterval(connIv);
            location.href='http://'+st.ip+'/success';
          }
        }).catch(function(){});
      },2000);
    } else {
      s.className='status error';
      s.textContent='保存失败:'+d.error;
    }
  }).catch(function(err){
    var s=document.getElementById('status');
    s.className='status error';
    s.textContent='请求失败:'+err;
  });
});
doScan();
</script>
</body>
</html>
)HTML";

static const char *auth_type_str[] = {
    "OPEN", "WEP", "WPA_PSK", "WPA2_PSK", "WPA_WPA2_PSK",
    "WPA2_ENTERPRISE", "WPA3_PSK", "WPA2_WPA3_PSK", "WAPI_PSK",
    "MAX"
};

/* 连接成功后展示的简单页面 */
static const char *SUCCESS_PAGE =
"<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>连接成功</title><style>"
"body{font-family:\"Microsoft YaHei\",sans-serif;background:linear-gradient(135deg,#667eea,#764ba2);"
"min-height:100vh;display:flex;align-items:center;justify-content:center;margin:0}"
".card{background:#fff;border-radius:12px;padding:32px 24px;width:88%;max-width:400px;"
"text-align:center;box-shadow:0 8px 32px rgba(0,0,0,.18)}"
".icon{width:64px;height:64px;border-radius:50%;background:#28a745;color:#fff;"
"font-size:36px;line-height:64px;margin:0 auto 16px}"
"h1{color:#28a745;font-size:22px;margin:0 0 8px}"
".ssid{color:#333;font-size:17px;margin:6px 0}"
".ip{color:#888;font-size:14px;margin:4px 0}"
"</style></head><body><div class=\"card\">"
"<div class=\"icon\">&#10003;</div>"
"<h1>连接成功</h1>"
"<div class=\"ssid\" id=\"ssid\">-</div>"
"<div class=\"ip\" id=\"ip\">-</div>"
"</div><script>fetch('/api/status',{cache:'no-store'})"
".then(function(r){return r.json();}).then(function(d){"
"if(d.connected){document.getElementById('ssid').textContent='WiFi：'+d.ssid;"
"document.getElementById('ip').textContent='IP：'+d.ip;}}).catch(function(){});"
"</script></body></html>";


static void set_json_response_headers(httpd_req_t *req) {

    httpd_resp_set_type(req, "application/json");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

}



static esp_err_t scan_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "GET /api/scan");
    set_json_response_headers(req);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");

    /* 同步阻塞扫描：直接等结果，无需轮询 */
    wifi_ap_record_t ap_list[20];
    uint16_t ap_count = 0;
    esp_err_t err = wifi_manager_scan_all(ap_list, 20, &ap_count);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "扫描失败: %s", esp_err_to_name(err));
        httpd_resp_sendstr(req, "{\"success\":false,\"error\":\"scan failed\"}");
        return ESP_OK;
    }

    /* 构建 JSON */
    static char response[3072];
    int off = snprintf(response, sizeof(response), "{\"success\":true,\"aps\":[");
    for (int i = 0; i < (int)ap_count && off > 0 && off < (int)sizeof(response) - 220; i++) {
        int auth_idx = ap_list[i].authmode < 9 ? ap_list[i].authmode : 8;
        int n = snprintf(response + off, sizeof(response) - off,
                         "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":\"%s\",\"channel\":%d}",
                         (i > 0 ? "," : ""),
                         (const char *)ap_list[i].ssid, ap_list[i].rssi,
                         auth_type_str[auth_idx], ap_list[i].primary);
        if (n < 0) break;
        off += n;
    }
    if (off > 0 && off < (int)sizeof(response) - 3) {
        snprintf(response + off, sizeof(response) - off, "]}");
    }

    ESP_LOGI(TAG, "scan response: %d APs, len=%d", ap_count, off);
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}




/**

 * @brief 根页面请求处理器

 * 

 * @param req HTTP请求句柄

 * @return esp_err_t ESP_OK表示成功

 */

static esp_err_t status_handler(httpd_req_t *req)
{
    set_json_response_headers(req);

    char buf[256];
    int off = 0;
    bool connected = wifi_manager_is_connected();

    if (connected) {
        wifi_status_t st;
        const char *ip  = "unknown";
        const char *ssid = "?";
        if (wifi_manager_get_status(&st) == ESP_OK) {
            ip   = st.ip_address;
            ssid = st.ssid;
        }
        off = snprintf(buf, sizeof(buf),
                       "{\"connected\":true,\"ip\":\"%s\",\"ssid\":\"%s\"}",
                       ip, ssid);
    } else {
        off = snprintf(buf, sizeof(buf), "{\"connected\":false}");
    }

    httpd_resp_send(req, buf, off);
    return ESP_OK;
}

static esp_err_t root_handler(httpd_req_t *req) {

    httpd_resp_set_type(req, "text/html");

    httpd_resp_send(req, WEB_CONFIG_PAGE, strlen(WEB_CONFIG_PAGE));

    return ESP_OK;

}

static esp_err_t success_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, SUCCESS_PAGE, strlen(SUCCESS_PAGE));
    return ESP_OK;
}

/* 前向声明 */
static esp_err_t wifi_webserver_start(void);
static esp_err_t wifi_webserver_stop(void);

/* ==================== 内部包装（供 wifi_manager.c 使用） ==================== */

esp_err_t wifi_webserver_start_internal(void)
{
    return wifi_webserver_start();
}

esp_err_t wifi_webserver_stop_internal(void)
{
    return wifi_webserver_stop();
}



/**

 * @brief 配置提交请求处理器

 * 

 * @param req HTTP请求句柄

 * @return esp_err_t ESP_OK表示成功

 * 

 * 处理流程：

 * 1. 读取请求体

 * 2. 解析JSON数据

 * 3. 提取SSID和密码

 * 4. 保存配置

 * 5. 返回响应

 */

static esp_err_t config_handler(httpd_req_t *req) {

    ESP_LOGI(TAG, "POST /config received (content_len=%d)", req->content_len);

    set_json_response_headers(req);



    char buf[256];

    int ret, remaining = req->content_len;



    /* 读取请求体 */

    if (remaining >= sizeof(buf)) {

        httpd_resp_send(req, "{\"error\":\"Request too large\"}", 30);

        return ESP_OK;

    }



    ret = httpd_req_recv(req, buf, remaining);

    if (ret <= 0) {

        httpd_resp_send(req, "{\"error\":\"Read failed\"}", 24);

        return ESP_OK;

    }

    buf[ret] = '\0';



    /* 解析SSID和密码（简单解析，假设格式为 {"ssid":"xxx","password":"yyy"}） */

    char ssid[32] = {0};

    char password[64] = {0};

    

    /* 查找SSID */

    char *ssid_start = strstr(buf, "\"ssid\"");

    if (ssid_start) {

        ssid_start = strchr(ssid_start, ':');

        if (ssid_start) {

            ssid_start = strchr(ssid_start, '"');

            if (ssid_start) {

                ssid_start++;

                char *ssid_end = strchr(ssid_start, '"');

                if (ssid_end) {

                    int len = ssid_end - ssid_start;

                    if (len < sizeof(ssid)) {

                        strncpy(ssid, ssid_start, len);

                        ssid[len] = '\0';

                    }

                }

            }

        }

    }



    /* 查找密码 */

    char *password_start = strstr(buf, "\"password\"");

    if (password_start) {

        password_start = strchr(password_start, ':');

        if (password_start) {

            password_start = strchr(password_start, '"');

            if (password_start) {

                password_start++;

                char *password_end = strchr(password_start, '"');

                if (password_end) {

                    int len = password_end - password_start;

                    if (len < sizeof(password)) {

                        strncpy(password, password_start, len);

                        password[len] = '\0';

                    }

                }

            }

        }

    }



    /* 检查SSID是否为空 */

    if (strlen(ssid) == 0) {

        httpd_resp_send(req, "{\"error\":\"SSID required\"}", 27);

        return ESP_OK;

    }



    /* 保存WIFI配置 */
    esp_err_t err = wifi_manager_save_config(ssid, password);

    if (err != ESP_OK) {

        httpd_resp_send(req, "{\"error\":\"Save failed\"}", 24);

        return ESP_OK;

    }



    /* 返回成功响应 */

    httpd_resp_send(req, "{\"success\":true}", 16);



    /* 在新任务中尝试连接WIFI */

    xTaskCreate(wifi_connect_wrapper, "wifi_connect_task",
                4096, NULL, 5, NULL);



    return ESP_OK;

}



/**

 * @brief URI处理器定义

 */

static const httpd_uri_t root_uri = {

    .uri = "/",

    .method = HTTP_GET,

    .handler = root_handler,

    .user_ctx = NULL

};



static const httpd_uri_t config_uri = {

    .uri = "/config",

    .method = HTTP_POST,

    .handler = config_handler,

    .user_ctx = NULL

};



static const httpd_uri_t scan_uri = {

    .uri = "/api/scan",

    .method = HTTP_GET,

    .handler = scan_handler,

    .user_ctx = NULL

};

static const httpd_uri_t status_uri = {

    .uri = "/api/status",

    .method = HTTP_GET,

    .handler = status_handler,

    .user_ctx = NULL

};

static const httpd_uri_t success_uri = {

    .uri = "/success",

    .method = HTTP_GET,

    .handler = success_handler,

    .user_ctx = NULL

};



static esp_err_t options_handler(httpd_req_t *req) {

    set_json_response_headers(req);

    httpd_resp_send(req, NULL, 0);

    return ESP_OK;

}



static const httpd_uri_t options_config_uri = {

    .uri = "/config",

    .method = HTTP_OPTIONS,

    .handler = options_handler,

    .user_ctx = NULL

};



static const httpd_uri_t options_scan_uri = {

    .uri = "/api/scan",

    .method = HTTP_OPTIONS,

    .handler = options_handler,

    .user_ctx = NULL

};



/**

 * @brief 启动Web配置服务器

 * 

 * @return esp_err_t ESP_OK表示成功，其他表示失败

 */

esp_err_t wifi_webserver_start(void) {

    if (s_server != NULL) {

        ESP_LOGW(TAG, "Web server already running");

        return ESP_OK;

    }



    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port = 80;

    config.ctrl_port = 32768;

    config.max_uri_handlers = 16;   /* 默认 8 个槽位不够（webserver 4 + captive_portal 9），扩到 16 */
    config.stack_size = 8192;       /* scan_handler 结果缓冲较大，默认 4KB 栈会溢出 */



    esp_err_t err = httpd_start(&s_server, &config);

    if (err != ESP_OK) {

        ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(err));

        return err;

    }



    /* 注册URI处理器 */

    httpd_register_uri_handler(s_server, &root_uri);

    httpd_register_uri_handler(s_server, &config_uri);

    httpd_register_uri_handler(s_server, &scan_uri);

    httpd_register_uri_handler(s_server, &status_uri);

    httpd_register_uri_handler(s_server, &success_uri);

    httpd_register_uri_handler(s_server, &options_config_uri);

    httpd_register_uri_handler(s_server, &options_scan_uri);



    ESP_LOGI(TAG, "Web server started on port 80");



    /* 启动 Captive Portal：手机/电脑连上热点后自动弹出配网页，无需手动访问 IP。

       AP 网关固定为 192.168.4.1（esp_wifi 内部默认网段） */

    esp_err_t ap_err = captive_portal_start("192.168.4.1");

    if (ap_err != ESP_OK) {

        ESP_LOGW(TAG, "Captive Portal 启动失败：%s", esp_err_to_name(ap_err));

    }



    return ESP_OK;

}



/**

 * @brief 停止Web配置服务器

 *

 * @return esp_err_t ESP_OK表示成功，其他表示失败

 */

esp_err_t wifi_webserver_stop(void) {

    if (s_server == NULL) {

        return ESP_OK;

    }



    /* 先停 Captive Portal 再停 HTTP：DNS 任务需在服务器停止前注销通配处理器 */

    captive_portal_stop();



    httpd_stop(s_server);

    s_server = NULL;



    ESP_LOGI(TAG, "Web server stopped");

    return ESP_OK;

}

