#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp_log.h"
#include "esp_dsp.h"

// Konfiguration
#define I2S_NUM             I2S_NUM_0
#define SAMPLE_RATE         44100  // 44.1 kHz
#define SAMPLES             441    // Sample-Länge
#define DMA_BUFFER_LENGTH   512    // Maximale Pufferlänge
#define DMA_BUFFER_COUNT    4      // Anzahl der DMA-Puffer
#define TAG                 "I2S_ADC_FFT"

// Buffers
static uint16_t adc_buffer[SAMPLES];
static float fft_real[SAMPLES];
static float fft_imag[SAMPLES];
static float fft_magnitude[SAMPLES / 2];

// Konfiguration von I2S für ADC-Eingang
void configure_i2s_adc() {
    // Konfiguriere ADC-Auflösung und Dämpfung
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);

    // I2S-Konfiguration
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUFFER_COUNT,
        .dma_buf_len = DMA_BUFFER_LENGTH,
        .use_apll = false
    };

    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_0));
    ESP_LOGI(TAG, "I2S ADC konfiguriert mit Abtastrate: %d Hz", SAMPLE_RATE);
}

// Führt die FFT aus und berechnet die Hauptfrequenz
void perform_fft() {
    // Kopiere ADC-Daten in das FFT-Puffer
    for (int i = 0; i < SAMPLES; i++) {
        fft_real[i] = (float)adc_buffer[i];
        fft_imag[i] = 0.0f;
    }

    // FFT durchführen
    ESP_LOGI(TAG, "FFT wird durchgeführt...");
    dsps_fft2r_fc32(fft_real, SAMPLES);
    dsps_bit_rev_fc32(fft_real, SAMPLES);
    dsps_cplx2real_fc32(fft_real, SAMPLES);

    // Berechnung der Amplituden
    for (int i = 0; i < SAMPLES / 2; i++) {
        fft_magnitude[i] = sqrtf(fft_real[i] * fft_real[i] + fft_imag[i] * fft_imag[i]);
    }

    // Suche die Frequenz mit der höchsten Amplitude
    float max_magnitude = 0.0f;
    int max_index = 0;
    for (int i = 0; i < SAMPLES / 2; i++) {
        if (fft_magnitude[i] > max_magnitude) {
            max_magnitude = fft_magnitude[i];
            max_index = i;
        }
    }

    float main_frequency = (float)max_index * SAMPLE_RATE / SAMPLES;
    ESP_LOGI(TAG, "Hauptfrequenz: %.2f Hz (Amplitude: %.2f)", main_frequency, max_magnitude);
}

// Task zum Lesen von ADC-Daten und Ausführen der FFT
void read_adc_data() {
    size_t bytes_read;
    while (1) {
        ESP_LOGI(TAG, "ADC-Daten werden gelesen...");
        ESP_ERROR_CHECK(i2s_read(I2S_NUM, adc_buffer, sizeof(adc_buffer), &bytes_read, portMAX_DELAY));

        ESP_LOGI(TAG, "Gelesene Bytes: %d", bytes_read);
        perform_fft();

        // Warte 5 Sekunden
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main() {
    ESP_LOGI(TAG, "Start der ADC-Abtastung und FFT...");

    // Initialisiere FFT-Tabellen
    dsps_fft2r_init_fc32(NULL, SAMPLES);

    // Konfiguriere I2S für ADC-Eingang
    configure_i2s_adc();

    // Starte die ADC-Leseaufgabe
    xTaskCreate(read_adc_data, "read_adc_data", 8192, NULL, 5, NULL);
}
