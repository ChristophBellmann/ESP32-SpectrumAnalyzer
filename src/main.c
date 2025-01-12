#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_dsp.h"
#include "esp_http_server.h"

#define LED_PIN 2
#define WIFI_SSID "Ultranet"
#define WIFI_PASS "Lassmichrein!"
#define MAXIMUM_RETRY 10

#define SAMPLE_RATE 44100
#define BUFFER_SIZE 1024
#define ADC_CHANNEL ADC1_CHANNEL_0

static const char *TAG = "WiFi_FFT";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int retry_count = 0;

// FFT buffers
__attribute__((aligned(16))) float adc_buffer[BUFFER_SIZE]; // ADC data
__attribute__((aligned(16))) float wind[BUFFER_SIZE];       // Hann window coefficients
__attribute__((aligned(16))) float y_cf[BUFFER_SIZE * 2];   // Complex buffer for FFT
float *y1_cf = &y_cf[0];

// HTTP Server Handlers
static esp_err_t root_handler(httpd_req_t *req) {
    const char *response = "ESP32 Web Server Running!";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root);
    }
    return server;
}

// WiFi Event Handler
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi gestartet, Verbindung wird hergestellt...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_count < MAXIMUM_RETRY) {
            esp_wifi_connect();
            retry_count++;
            ESP_LOGW(TAG, "Verbindung fehlgeschlagen. Neuer Versuch (%d/%d)...", retry_count, MAXIMUM_RETRY);
        } else {
            ESP_LOGE(TAG, "Maximale Verbindungsversuche erreicht.");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Erfolgreich verbunden. IP-Adresse: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void connect_wifi(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialisiert, versuche Verbindung zu SSID: %s", WIFI_SSID);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Erfolgreich verbunden mit SSID: %s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Fehler beim Verbinden mit SSID: %s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "Unbekanntes Ereignis");
    }

    vEventGroupDelete(s_wifi_event_group);
}

// ADC and FFT Task
void adc_fft_task(void *arg) {
    ESP_LOGI(TAG, "Initialisiere Hann-Fenster und FFT...");
    dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    dsps_wind_hann_f32(wind, BUFFER_SIZE);

    vTaskDelay(pdMS_TO_TICKS(10000)); // Wait 10 seconds before starting FFT

    while (1) {
        ESP_LOGI(TAG, "Starte ADC-Abtastung...");
        for (int i = 0; i < BUFFER_SIZE; i++) {
            adc_buffer[i] = (float)adc1_get_raw(ADC_CHANNEL);
            esp_rom_delay_us(1000000 / SAMPLE_RATE);

            if (i % 100 == 0) {
                vTaskDelay(pdMS_TO_TICKS(1)); // Allow other tasks
            }
        }

        ESP_LOGI(TAG, "ADC-Abtastung abgeschlossen. Starte FFT-Analyse...");

        // Apply Hann window and convert to complex values
        for (int i = 0; i < BUFFER_SIZE; i++) {
            y_cf[i * 2] = adc_buffer[i] * wind[i]; // Real part
            y_cf[i * 2 + 1] = 0;                  // Imaginary part
        }

        // Perform FFT
        unsigned int start_b = dsp_get_cpu_cycle_count();
        dsps_fft2r_fc32(y_cf, BUFFER_SIZE);
        dsps_bit_rev_fc32(y_cf, BUFFER_SIZE);
        dsps_cplx2reC_fc32(y_cf, BUFFER_SIZE);
        unsigned int end_b = dsp_get_cpu_cycle_count();

        ESP_LOGI(TAG, "FFT abgeschlossen. Verarbeitungszeit: %u Zyklen", end_b - start_b);

        vTaskDelay(pdMS_TO_TICKS(2000)); // Run every 2 seconds
    }
}

void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_rom_gpio_pad_select_gpio(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    connect_wifi();

    ESP_LOGI(TAG, "Starting HTTP server...");
    start_webserver();

    ESP_LOGI(TAG, "Starte ADC- und FFT-Task...");
    xTaskCreate(adc_fft_task, "adc_fft_task", 8192, NULL, 5, NULL);
}