// streamServer.cpp

#include "streamServer.hpp"
#include <Arduino.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include <esp_camera.h>

// Boundary for multipart MJPEG stream
#define PART_BOUNDARY "123456789000000000000987654321"
constexpr static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
constexpr static const char *STREAM_BOUNDARY      = "\r\n--" PART_BOUNDARY "\r\n";
constexpr static const char *STREAM_PART          = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %lld.%06lld\r\n\r\n";

// Forward‐declare the global server handle used in this file
static httpd_handle_t camera_stream = nullptr;

//------------------------------------------------------------------------------
// Stream handler (never returns)
//------------------------------------------------------------------------------
esp_err_t StreamHelpers::stream(httpd_req_t *req)
{
    camera_fb_t *fb = nullptr;
    struct timeval timestamp;

    // tell client we're doing MJPEG over HTTP
    esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");

    while (true)
    {
        fb = esp_camera_fb_get();
        if (!fb) {
            log_e("Camera capture failed");
            res = ESP_FAIL;
        } else {
            timestamp.tv_sec  = fb->timestamp.tv_sec;
            timestamp.tv_usec = fb->timestamp.tv_usec;
        }

        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        }
        if (res == ESP_OK && fb) {
            // header for this part
            char hdr[128];
            int hlen = snprintf(hdr, sizeof(hdr), STREAM_PART,
                                fb->len,
                                (long long)timestamp.tv_sec,
                                (long long)timestamp.tv_usec);
            res = httpd_resp_send_chunk(req, hdr, hlen);
        }
        if (res == ESP_OK && fb) {
            res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        }

        // return the frame buffer
        if (fb) {
            esp_camera_fb_return(fb);
            fb = nullptr;
        }

        if (res != ESP_OK) break;
    }

    return res;
}

//------------------------------------------------------------------------------
// Ping handler (simple, quick response)
//------------------------------------------------------------------------------
static esp_err_t ping_handler(httpd_req_t *req)
{
    const char* resp = "pong";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}
static httpd_uri_t ping_page = {
    .uri      = "/ping",
    .method   = HTTP_GET,
    .handler  = ping_handler,
    .user_ctx = nullptr
};

//------------------------------------------------------------------------------
// Task to start a tiny HTTPD instance for just /ping
//------------------------------------------------------------------------------
static void pingServerTask(void* /*pvParameters*/)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 81;
    cfg.ctrl_port   = 81;
    cfg.core_id     = 0;      // pin ping server to core 0

    httpd_handle_t ping_server = nullptr;
    if (httpd_start(&ping_server, &cfg) == ESP_OK) {
        httpd_register_uri_handler(ping_server, &ping_page);
        Serial.printf("Ping server running on port %d\n", cfg.server_port);
    } else {
        Serial.println("Failed to start ping server");
    }

    // No loop needed—this task has done its job
    vTaskDelete(nullptr);
}

//------------------------------------------------------------------------------
// StreamServer class
//------------------------------------------------------------------------------
StreamServer::StreamServer(const int STREAM_PORT)
  : STREAM_SERVER_PORT(STREAM_PORT)
{}

int StreamServer::startStreamServer()
{
    // 1) Launch the ping server task before the stream
    xTaskCreatePinnedToCore(
        pingServerTask,
        "PingTask",
        4096,        // stack size
        nullptr,
        1,           // priority
        nullptr,
        0            // core 0
    );

    // 2) Configure & start the MJPEG stream server on port STREAM_SERVER_PORT
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port      = STREAM_SERVER_PORT;
    config.ctrl_port        = STREAM_SERVER_PORT;
    config.core_id          = 1;      // pin stream server to core 1
    config.stack_size       = 20480;
    config.max_uri_handlers = 10;

    // Start the server
    esp_err_t status = httpd_start(&camera_stream, &config);
    if (status != ESP_OK) {
        Serial.println("Failed to start stream server");
        return -1;
    }

    // Register the stream handler on “/”
    static httpd_uri_t stream_page = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = &StreamHelpers::stream,
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(camera_stream, &stream_page);

    Serial.println("Stream server initialized on port " + String(STREAM_SERVER_PORT));
    IPAddress ip = (wifiStateManager.getCurrentState() == WiFiState_e::WiFiState_ADHOC)
                   ? WiFi.softAPIP()
                   : WiFi.localIP();
    Serial.printf("Stream available at http://%s:%d\n", ip.toString().c_str(), STREAM_SERVER_PORT);

    return 0;
}
