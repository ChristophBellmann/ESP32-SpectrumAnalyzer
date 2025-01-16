#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "config.h"

static const char *TAG = "ADC_FFT";

__attribute__((aligned(16))) int16_t adc_buffer[FFT_SIZE];
__attribute__((aligned(16))) float fft_input[FFT_SIZE * 2];
__attribute__((aligned(16))) float window[FFT_SIZE];
adc_continuous_handle_t adc_handle = NULL;
float main_frequency = 0.0;
float max_magnitude = 0.0;

void configure_adc_continuous() {
    adc_continuous_handle_cfg_t continuous_cfg = {
        .max_store_buf_size = FFT_SIZE * sizeof(int16_t),
        .conv_frame_size = FFT_SIZE,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&continuous_cfg, &adc_handle));

    adc_continuous_config_t adc_config = {
        .sample_freq_hz = SAMPLE_RATE,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
        .pattern_num = 1,
    };

    adc_digi_pattern_config_t adc_pattern[1] = {
        {
            .atten = ADC_ATTEN,
            .channel = ADC_CHANNEL,
            .unit = ADC_UNIT_1,
            .bit_width = ADC_BITWIDTH_12,
        }
    };

    adc_config.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &adc_config));
    ESP_LOGI(TAG, "ADC continuous mode configured");
}

void perform_fft() {
    ESP_LOGI(TAG, "Initializing FFT...");
    dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    dsps_wind_hann_f32(window, FFT_SIZE);

    for (int i = 0; i < FFT_SIZE; i++) {
        fft_input[i * 2] = adc_buffer[i] * window[i];
        fft_input[i * 2 + 1] = 0.0f;
    }

    dsps_fft2r_fc32(fft_input, FFT_SIZE);
    dsps_bit_rev_fc32(fft_input, FFT_SIZE);
    dsps_cplx2reC_fc32(fft_input, FFT_SIZE);

    max_magnitude = 0.0;
    int max_index = 0;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        float magnitude = sqrt(fft_input[i * 2] * fft_input[i * 2] +
                               fft_input[i * 2 + 1] * fft_input[i * 2 + 1]);
        if (magnitude > max_magnitude) {
            max_magnitude = magnitude;
            max_index = i;
        }
    }

    main_frequency = (float)max_index * SAMPLE_RATE / FFT_SIZE;
    ESP_LOGI(TAG, "Main Frequency: %.2f Hz, Magnitude: %.2f", main_frequency, max_magnitude);
}

void collect_adc_continuous_data() {
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
    ESP_LOGI(TAG, "ADC started in continuous mode");

    size_t bytes_read = 0;

    while (1) {
        ESP_ERROR_CHECK(adc_continuous_read(adc_handle, (uint8_t *)adc_buffer, sizeof(adc_buffer), &bytes_read, portMAX_DELAY));
        if (bytes_read > 0) {
            ESP_LOGI(TAG, "Collected %d bytes of ADC data", bytes_read);
            perform_fft();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
