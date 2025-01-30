// wifi.c

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_server.h"

#include "config.h"        // for WIFI_SSID, WIFI_PASS
#include "wifi.h"
#include "fastdetect.h"    // for fastdetect_handler, ws_handler, etc.
#include "adc_fft.h"       // if needed
#include "wav.h"           // if needed

static const char *TAG = "WiFi";
static bool wifi_connected = false;
static httpd_handle_t s_http_server = NULL;

// -------------------------
// Wi-Fi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        ESP_LOGI(TAG, "Wi-Fi disconnected. Reconnecting...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        char ip_str[16];
        esp_ip4addr_ntoa(&ev->ip_info.ip, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "Got IP: %s", ip_str);
    }
}

// -------------------------
// Initialize Wi-Fi in STA mode
void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                        WIFI_EVENT, ESP_EVENT_ANY_ID,
                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
                        IP_EVENT, IP_EVENT_STA_GOT_IP,
                        &wifi_event_handler, NULL, NULL));

    // Configure Wi-Fi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    esp_wifi_connect();

    // Wait for connection
    while (!wifi_connected) {
        ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Wi-Fi connected. (No background WS push, client must poll)");
}

// -------------------------
// Favicon handler
static esp_err_t favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

// Example main-frequency handler (optional)
extern float main_frequency;
extern float max_magnitude;
static esp_err_t main_frequency_handler(httpd_req_t *req)
{
    char response[512];
    snprintf(response, sizeof(response),
             "<html>"
             "<body>"
             "<h1>Main Frequency: %.2f Hz</h1>"
             "<h2>Magnitude: %.2f</h2>"
             "</body>"
             "</html>",
             main_frequency, max_magnitude);
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

// -------------------------
// Handler for root '/' serving index.html
#define INDEX_HTML_PATH  "/spiffs/index.html"

/**
 * @brief Handler for serving the index.html page from SPIFFS at '/'.
 *
 * @param req HTTP request handler.
 * @return esp_err_t ESP_OK on success.
 */
static esp_err_t index_handler(httpd_req_t *req)
{
    FILE *f = fopen(INDEX_HTML_PATH, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", INDEX_HTML_PATH);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "HTML file not found");
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        ESP_LOGE(TAG, "Failed to determine size of %s", INDEX_HTML_PATH);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ftell failed");
        return ESP_FAIL;
    }

    char *buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(f);
        ESP_LOGE(TAG, "Failed to allocate memory for HTML file.");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    fread(buf, 1, size, f);
    fclose(f);
    buf[size] = '\0';

    httpd_resp_set_type(req, "text/html");
    esp_err_t res = httpd_resp_send(req, buf, size);

    free(buf);
    return res;
}

// -------------------------
// Start the webserver
httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        s_http_server = server;

        // 1) /favicon.ico => no content
        httpd_uri_t favicon_uri = {
            .uri      = "/favicon.ico",
            .method   = HTTP_GET,
            .handler  = favicon_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &favicon_uri);

        // 2) /fastdetect => HTML
        httpd_uri_t fastdetect_html_uri = {
            .uri      = "/fastdetect",
            .method   = HTTP_GET,
            .handler  = fastdetect_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &fastdetect_html_uri);

        // 3) /ws => WebSocket
        httpd_uri_t ws_uri = {
            .uri          = "/ws",
            .method       = HTTP_GET,
            .handler      = ws_handler,
            .user_ctx     = NULL,
            .is_websocket = true
        };
        httpd_register_uri_handler(server, &ws_uri);

        // 4) / => index.html
        httpd_uri_t index_html_uri = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = index_handler, // New handler for root
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &index_html_uri);

        // 5) /freq => optional
        // Uncomment if needed
        /*
        httpd_uri_t freq_uri = {
            .uri      = "/freq",
            .method   = HTTP_GET,
            .handler  = main_frequency_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &freq_uri);
        */

        ESP_LOGI(TAG, "Webserver started on port %d", config.server_port);
    } else {
        ESP_LOGE(TAG, "Error starting HTTP server!");
    }

    return server;
}
