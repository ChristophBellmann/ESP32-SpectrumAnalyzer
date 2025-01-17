#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "config.h"
#include "wifi.h"
#include "adc_fft.h" // Für main_frequency und max_magnitude
#include "wav.h"

static const char *TAG = "WiFi";
static bool wifi_connected = false;
extern float main_frequency;
extern float max_magnitude;

// Wi-Fi Event Handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        ESP_LOGI(TAG, "Wi-Fi disconnected. Reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip_str[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
        ESP_LOGI(TAG, "Got IP: %s", ip_str);
    }
}

// Initialize Wi-Fi
void wifi_init_sta() {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    esp_wifi_connect();

    while (!wifi_connected) {
        ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t main_frequency_handler(httpd_req_t *req) {
    char response[1024];
    snprintf(response, sizeof(response),
             "<html>"
             "<body>"
             "<h1>Main Frequency: %.2f Hz</h1>"
             "<h2>Magnitude: %.2f</h2>"
             "<audio controls src='/download?timestamp=%lu'></audio>"
             "<br>"
             "<button onclick=\"window.location.href='/download?timestamp=%lu'\">Download WAV</button>"
             "</body>"
             "</html>",
             main_frequency, max_magnitude, esp_log_timestamp(), esp_log_timestamp());

    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}


// Start Webserver
httpd_handle_t start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_main = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = main_frequency_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_main);

        httpd_uri_t uri_download = {
            .uri = "/download",
            .method = HTTP_GET,
            .handler = wav_download_handler, // WAV-Download-Handler
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_download);
    }

    return server;
}
