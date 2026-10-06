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

#include "wifi_webserver_internal.h"
#include "captive_portal.h"

static const char *TAG = "wifi_webserver";
static httpd_handle_t s_server = NULL;

httpd_handle_t wifi_webserver_handle(void) { return s_server; }

static void wifi_connect_wrapper(void *arg) {
    (void)arg;
    wifi_connect();
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
.btn-save { width:100%; padding:12px; background:#007bff; color:#fff; border:none; border-radius:8px; font-size:16px; cursor:pointer; margin-bottom:10px; }
.btn-save:hover { background:#0056b3; }
label { display:block; margin:10px 0 4px; color:#555; font-size:14px; }
input[type=text],input[type=password] { width:100%; padding:10px 12px; border:1px solid #ccc; border-radius:6px; font-size:15px; }
.wifi-list { max-height:220px; overflow-y:auto; margin-bottom:12px; }
.wifi-item { padding:10px 12px; border:1px solid #ddd; border-radius:6px; margin-bottom:6px; cursor:pointer; }
.wifi-item:hover { background:#e9f5ff; }
.wifi-item.selected { background:#cce5ff; border-color:#007bff; }
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
<div id="wifi-list" class="wifi-list"></div>
<div id="scanning" class="scanning">正在扫描...</div>
<form id="configForm">
<label for="ssid">WiFi 名称</label>
<input type="text" id="ssid" name="ssid" required>
<label for="password">密码</label>
<input type="password" id="password" name="password">
<button type="submit" class="btn-save">保存并连接</button>
</form>
<button class="btn-scan" onclick="doScan()">扫描可用 WiFi</button>
<div id="status" class="status"></div>
</div>
<script>
var sel='',scanIv=null,scanTry=0,MAX=15;
function doScan(){
  document.getElementById('scanning').style.display='block';
  document.getElementById('wifi-list').innerHTML='';
  if(scanIv) clearInterval(scanIv);
  scanTry=0; fetchScan();
}
function fetchScan(){
  scanTry++;
  if(scanTry>MAX){
    document.getElementById('scanning').style.display='none';
    if(scanIv) clearInterval(scanIv);
    document.getElementById('wifi-list').innerHTML='<div style="text-align:center;color:red">扫描超时，请重试</div>';
    return;
  }
  fetch('/api/scan').then(function(r){return r.json();})
  .then(function(d){
    if(d.success&&d.aps){
      document.getElementById('scanning').style.display='none';
      if(scanIv) clearInterval(scanIv);
      if(d.aps.length>0){
        var h='';
        d.aps.forEach(function(a){
          h+='<div class="wifi-item" onclick="pick(\''+a.ssid+'\')">';
          h+='<div class="wifi-name">'+a.ssid+'</div>';
          h+='<div class="wifi-info">信号:'+a.rssi+' dBm | '+a.auth+'</div>';
          h+='</div>';
        });
        document.getElementById('wifi-list').innerHTML=h;
      } else {
        document.getElementById('wifi-list').innerHTML='<div style="text-align:center;color:#888">未扫描到可用的WiFi</div>';
      }
    } else if(d.scanning){
      scanIv=setTimeout(fetchScan,1000);
    } else {
      document.getElementById('scanning').style.display='none';
      if(scanIv) clearInterval(scanIv);
      document.getElementById('wifi-list').innerHTML='<div style="text-align:center;color:red">扫描失败</div>';
    }
  }).catch(function(){
    document.getElementById('scanning').style.display='none';
    if(scanIv) clearInterval(scanIv);
    document.getElementById('wifi-list').innerHTML='<div style="text-align:center;color:red">请求失败</div>';
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
      s.textContent='配置已保存，正在连接...';
      if(scanIv) clearInterval(scanIv);
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
fetchScan();
</script>
</body>
</html>
)HTML";

static const char *auth_type_str[] = {

    "OPEN", "WEP", "WPA_PSK", "WPA2_PSK", "WPA_WPA2_PSK", 

    "WPA2_ENTERPRISE", "WPA3_PSK", "WPA2_WPA3_PSK", "WAPI_PSK",

    "MAX"

};



static void set_json_response_headers(httpd_req_t *req) {

    httpd_resp_set_type(req, "application/json");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

}



static esp_err_t scan_handler(httpd_req_t *req) {

    ESP_LOGI(TAG, "GET /api/scan received (content_len=%d)", req->content_len);

    set_json_response_headers(req);



    wifi_ap_record_t ap_list[20];

    uint16_t ap_count = 0;

    esp_err_t err = wifi_scan_get_ap_results(ap_list, 20, &ap_count);



    if (err == ESP_ERR_NOT_FINISHED) {

        /* 扫描进行中，返回 scanning:true 让 JS 继续轮询 */

        httpd_resp_send(req, "{\"success\":false,\"scanning\":true}", 43);

        return ESP_OK;

    }



    if (err == ESP_ERR_NOT_FOUND) {

        /* 尚未启动过扫描，或上次扫描已结束但结果已被消费，启动新的非阻塞扫描 */

        esp_err_t start_err = wifi_scan_start_nonblocking();

        if (start_err != ESP_OK) {

            /* 扫描已在进行或其他错误：如果 s_scanning=true，说明上一次启动成功了，

               但本条件块只在 NOT_FOUND 时进入（s_scanning==false && s_scan_completed==false），

               所以这里大概率是 scan already in progress，返回 scanning:true 让 JS 继续轮询 */

            ESP_LOGW(TAG, "Scan start failed: %s, will retry on next poll", esp_err_to_name(start_err));

            httpd_resp_send(req, "{\"success\":false,\"scanning\":true}", 43);

            return ESP_OK;

        }

        httpd_resp_send(req, "{\"success\":false,\"scanning\":true}", 43);

        return ESP_OK;

    }



    if (err != ESP_OK) {

        httpd_resp_send(req, "{\"success\":false,\"error\":\"Scan failed\"}", 42);

        return ESP_OK;

    }



    /* 扫描完成，返回结果 */

    char response[2048] = "{\"success\":true,\"aps\":[";

    for (int i = 0; i < ap_count; i++) {

        if (i > 0) strcat(response, ",");



        int auth_idx = ap_list[i].authmode < 9 ? ap_list[i].authmode : 8;

        char ap_json[200];

        snprintf(ap_json, sizeof(ap_json),

                 "{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":\"%s\",\"channel\":%d}",

                 ap_list[i].ssid, ap_list[i].rssi,

                 auth_type_str[auth_idx], ap_list[i].primary);

        strcat(response, ap_json);

    }

    strcat(response, "]}");



    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;

}



/**

 * @brief 根页面请求处理器

 * 

 * @param req HTTP请求句柄

 * @return esp_err_t ESP_OK表示成功

 */

static esp_err_t root_handler(httpd_req_t *req) {

    httpd_resp_set_type(req, "text/html");

    httpd_resp_send(req, WEB_CONFIG_PAGE, strlen(WEB_CONFIG_PAGE));

    return ESP_OK;

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



    /* 构建WIFI配置结构体 */

    wifi_saved_config_t config = {

        .saved = true,

    };

    strncpy(config.ssid, ssid, sizeof(config.ssid) - 1);

    strncpy(config.password, password, sizeof(config.password) - 1);



    /* 保存WIFI配置 */

    esp_err_t err = wifi_config_save(&config);

    if (err != ESP_OK) {

        httpd_resp_send(req, "{\"error\":\"Save failed\"}", 24);

        return ESP_OK;

    }



    /* 返回成功响应 */

    httpd_resp_send(req, "{\"success\":true}", 16);



    /* 在新任务中尝试连接WIFI */

    xTaskCreatePinnedToCore((void(*)(void*))wifi_connect_wrapper, "wifi_connect_task", 

                            4096, NULL, 5, NULL, 0);



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



    esp_err_t err = httpd_start(&s_server, &config);

    if (err != ESP_OK) {

        ESP_LOGE(TAG, "Failed to start web server: %s", esp_err_to_name(err));

        return err;

    }



    /* 注册URI处理器 */

    httpd_register_uri_handler(s_server, &root_uri);

    httpd_register_uri_handler(s_server, &config_uri);

    httpd_register_uri_handler(s_server, &scan_uri);

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

