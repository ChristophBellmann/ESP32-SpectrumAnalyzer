#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_adc/adc_continuous.h"
#include "esp_http_server.h"
#include "config.h"
#include "adc_fft.h"
#include "wav.h"

static const char *TAG = "WAV";

#define NUM_BUFFERS 50
#define HEADER_SIZE 44
static int16_t collected_data[NUM_BUFFERS][FFT_SIZE];
static SemaphoreHandle_t adc_semaphore = NULL;

// Funktion zum Erstellen des WAV-Headers
void generate_wav_header(uint8_t *header, uint32_t data_size) {
    uint32_t sample_rate = SAMPLE_RATE;
    uint16_t bits_per_sample = 16;
    uint16_t num_channels = 1;
    uint32_t byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
    uint16_t block_align = num_channels * (bits_per_sample / 8);

    memcpy(header, "RIFF", 4);                            // ChunkID
    *((uint32_t *)(header + 4)) = 36 + data_size;         // ChunkSize
    memcpy(header + 8, "WAVE", 4);                       // Format
    memcpy(header + 12, "fmt ", 4);                      // Subchunk1ID
    *((uint32_t *)(header + 16)) = 16;                   // Subchunk1Size
    *((uint16_t *)(header + 20)) = 1;                    // AudioFormat (PCM)
    *((uint16_t *)(header + 22)) = num_channels;         // NumChannels
    *((uint32_t *)(header + 24)) = sample_rate;          // SampleRate
    *((uint32_t *)(header + 28)) = byte_rate;            // ByteRate
    *((uint16_t *)(header + 32)) = block_align;          // BlockAlign
    *((uint16_t *)(header + 34)) = bits_per_sample;      // BitsPerSample
    memcpy(header + 36, "data", 4);                     // Subchunk2ID
    *((uint32_t *)(header + 40)) = data_size;            // Subchunk2Size
}

// Funktion zur Speicherung von 50 Buffers
void save_adc_data() {
    ESP_LOGI(TAG, "Saving %d ADC buffers...", NUM_BUFFERS);

    if (adc_semaphore == NULL) {
        adc_semaphore = xSemaphoreCreateBinary();
        if (adc_semaphore == NULL) {
            ESP_LOGE(TAG, "Failed to create ADC semaphore");
            return;
        }
    }

    xSemaphoreTake(adc_semaphore, portMAX_DELAY);

    size_t bytes_read = 0;
    for (int i = 0; i < NUM_BUFFERS; i++) {
        ESP_ERROR_CHECK(adc_continuous_read(adc_handle, (uint8_t *)collected_data[i], sizeof(collected_data[i]), &bytes_read, portMAX_DELAY));
        if (bytes_read > 0) {
            ESP_LOGI(TAG, "Buffer %d/%d saved", i + 1, NUM_BUFFERS);
        } else {
            ESP_LOGE(TAG, "Failed to read ADC data for buffer %d", i + 1);
        }
    }

    xSemaphoreGive(adc_semaphore);

    ESP_LOGI(TAG, "All %d ADC buffers saved successfully.", NUM_BUFFERS);
}

// HTTP-Handler für den Download-Button
esp_err_t wav_download_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Download request received. Generating WAV file...");

    // ADC-Speicherung starten
    save_adc_data();

    // WAV-Header erstellen
    uint32_t data_size = NUM_BUFFERS * FFT_SIZE * sizeof(int16_t);
    uint8_t wav_header[HEADER_SIZE];
    generate_wav_header(wav_header, data_size);

    // Gesamte WAV-Daten zusammenstellen
    size_t wav_size = HEADER_SIZE + data_size;
    uint8_t *wav_data = (uint8_t *)malloc(wav_size);
    if (wav_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for WAV data");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory for WAV file");
        return ESP_FAIL;
    }

    memcpy(wav_data, wav_header, HEADER_SIZE);
    memcpy(wav_data + HEADER_SIZE, collected_data, data_size);

    // WAV-Daten senden
    httpd_resp_set_type(req, "audio/wav");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=adc_data.wav");
    esp_err_t res = httpd_resp_send(req, (const char *)wav_data, wav_size);

    free(wav_data);

    if (res == ESP_OK) {
        ESP_LOGI(TAG, "WAV data sent to client.");
    } else {
        ESP_LOGE(TAG, "Failed to send WAV data.");
    }

    return res;
}
