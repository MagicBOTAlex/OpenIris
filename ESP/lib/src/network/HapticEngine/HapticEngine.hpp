#pragma once
#ifndef HAPTIC_ENGINE_HPP
#define HAPTIC_ENGINE_HPP
#define PART_BOUNDARY "123456789000000000000987654321"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_http_server.h"
#include "HapticInstance.hpp"

class HapticEngine
{
private:
    static esp_err_t ping_handler(httpd_req_t *req)
    {
        auto *self = reinterpret_cast<HapticEngine*>(req->user_ctx);
        return self->doPing(req);
    }
    
    esp_err_t doPing(httpd_req_t *req)
    {
        const char* resp = "pong";
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }
public:

    void pingServerTask(void* /*pvParameters*/)
    {
        httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
        cfg.server_port = 81;
        cfg.ctrl_port   = 81;
        cfg.core_id     = 0;

        httpd_uri_t ping_page = {
            .uri      = "/ping",
            .method   = HTTP_GET,
            .handler  = ping_handler,
            .user_ctx = this
        };


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
};

#endif // HAPTIC_ENGINE_HPP
