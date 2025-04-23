#pragma once
#ifndef HAPTIC_ENGINE_HPP
#define HAPTIC_ENGINE_HPP
#define PART_BOUNDARY "123456789000000000000987654321"
#include <Arduino.h>
#include <WiFi.h>
#include "esp_http_server.h"
#include "HapticInstance.hpp"
#include <vector>

class HapticEngine
{
private:
    std::vector<HapticInstance> instances;
    httpd_handle_t server;
    httpd_config_t serverConf;

    static esp_err_t ping_handler(httpd_req_t *req)
    {
        auto *self = reinterpret_cast<HapticEngine*>(req->user_ctx);
        return self->doPing(req);
    }

    esp_err_t doPing(httpd_req_t *req)
    {
        const char* resp = "pong";
        setCorsHeaders(req);
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }


    // Idk how to do this better. this works
    static esp_err_t hapticEngineStatus_handler(httpd_req_t *req)
    {
        auto *self = reinterpret_cast<HapticEngine*>(req->user_ctx);
        return self->doHapticEngineStatus(req);
    }

    esp_err_t doHapticEngineStatus(httpd_req_t *req)
    {
        const char* resp = "Haptic engine is running";
        setCorsHeaders(req);
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }

    
    // Static handler – will be shared by all endpoints
    static esp_err_t handleHapticInstance(httpd_req_t *req) {
        // Recover the instance pointer we stored in user_ctx
        HapticInstance* inst = static_cast<HapticInstance*>(req->user_ctx);
        setCorsHeaders(req);

        // Only allow POST
        if (req->method != HTTP_POST) {
            httpd_resp_set_status(req, "405 Method Not Allowed");
            return httpd_resp_send(req, nullptr, 0);
        }

        // Read the JSON body
        int len = req->content_len;
        if (len <= 0) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        std::string body;
        body.resize(len);
        int ret = httpd_req_recv(req, &body[0], len);
        if (ret <= 0) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        // Parse it
        DynamicJsonDocument doc(256);
        auto err = deserializeJson(doc, body);
        if (err) {
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_send(req, "Invalid JSON", HTTPD_RESP_USE_STRLEN);
        }

        // Update strength if present
        if (doc.containsKey("strength")) {
            inst->strength = doc["strength"].as<int>();
            Serial.printf("[%s] Got strength: %d\n", inst->name, inst->strength);
            inst->updateInstance();
            return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
        } else {
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_send(req, "Missing 'strength'", HTTPD_RESP_USE_STRLEN);
        }
    }

    // Inject the CORS headers needed by browsers
    static void setCorsHeaders(httpd_req_t* req) {
        // allow your dev origin; or use "*" to allow any
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "http://localhost:1420");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    }

    static esp_err_t cors_handler(httpd_req_t* req) {
        setCorsHeaders(req);
        // no body needed
        return httpd_resp_send(req, nullptr, 0);
    }

    void setupHapticInstancesApi(std::vector<HapticInstance>& inst) {
        for (auto& instance : inst) {
            // build a path like "/motor1"
            String path = "/" + instance.name;

            httpd_uri_t instance_page = {
                .uri      = path.c_str(),
                .method   = HTTP_POST,                   // expect a JSON POST
                .handler  = handleHapticInstance,        // our shared handler
                .user_ctx = &instance                    // pointer to this instance
            };

            // register on your HTTP server
            httpd_register_uri_handler(server, &instance_page);

            Serial.printf(
                "Haptic instance \"%s\" listening on port %d at \"%s\"\n",
                instance.name,
                serverConf.server_port,
                path.c_str()
            );
        }
    }

    void updateHaptics(std::vector<HapticInstance>& inst){
        for (auto& instance : inst){
            instance.updateInstance();
        }
    }
    
public:

    void runTask(void* /*pvParameters*/)
    {
        // Just gonna hard code this for now
        instances.reserve(4);

        instances.emplace_back(2, "RightEar");
        instances.emplace_back(3, "LeftEar");
        instances.emplace_back(4, "None1");
        instances.emplace_back(5, "None2");

        
        updateHaptics(instances);

        serverConf = HTTPD_DEFAULT_CONFIG();
        serverConf.server_port = 82;
        serverConf.ctrl_port   = 82;
        serverConf.core_id     = 0;

        httpd_uri_t ping_page = {
            .uri      = "/ping",
            .method   = HTTP_GET,
            .handler  = ping_handler,
            .user_ctx = this
        };

        httpd_uri_t status_page = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = hapticEngineStatus_handler,
            .user_ctx = this
        };

        httpd_uri_t cors_uri = {
            .uri      = "/*",
            .method   = HTTP_OPTIONS,
            .handler  = cors_handler,
            .user_ctx = this
          };
        httpd_register_uri_handler(server, &cors_uri);

        // Haptics pages is generated dynamically
        
        server = nullptr;
        if (httpd_start(&server, &serverConf) == ESP_OK) {
            setupHapticInstancesApi(instances);
            
            httpd_register_uri_handler(server, &ping_page);
            Serial.printf("Ping running on port %d at: \"/ping\"", serverConf.server_port);
            httpd_register_uri_handler(server, &status_page);
            Serial.printf("HapticEngine check status running on port %d at: \"/\"", serverConf.server_port);
        } else {
            Serial.println("Failed to start ping server");
        }
        

        // while (true)
        // {
            
        // }
        vTaskDelete(nullptr);
    }
};

#endif // HAPTIC_ENGINE_HPP
