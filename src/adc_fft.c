#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc/adc_continuous.h"
#include "esp_log.h"
#include "esp_dsp.h"
#include "config.h"
#include "freertos/semphr.h" // Ensure this include exists
#include "fastdetect.h"

extern int16_t collected_data[NUM_BUFFERS][FFT_SIZE];
extern int current_buffer_index;
static SemaphoreHandle_t adc_semaphore = NULL;

static const char *TAG = "ADC_FFT";

__attribute__((aligned(16))) int16_t adc_buffer[FFT_SIZE];
__attribute__((aligned(16))) float fft_input[FFT_SIZE * 2];
__attribute__((aligned(16))) float window[FFT_SIZE];
__attribute__((aligned(16))) float detrended_data[FFT_SIZE]; // For DC removal
adc_continuous_handle_t adc_handle = NULL;
float main_frequency = 0.0;
float max_magnitude = 0.0;

// Shared ring buffer and current buffer index
int16_t collected_data[NUM_BUFFERS][FFT_SIZE];
int current_buffer_index = 0;

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

    #if ENABLE_ADC_FFT_LOGS
        ESP_LOGI(TAG, "Performing FFT...");
    #endif

    // Calculate mean for DC removal
    float mean = 0.0f;
    for (int i = 0; i < FFT_SIZE; i++) {
        mean += adc_buffer[i];
    }
    mean /= FFT_SIZE;

    // Remove DC component
    for (int i = 0; i < FFT_SIZE; i++) {
        detrended_data[i] = adc_buffer[i] - mean;
    }

    // Initialize FFT
    dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    dsps_wind_hann_f32(window, FFT_SIZE);

    // Apply Hann window and prepare FFT input
    for (int i = 0; i < FFT_SIZE; i++) {
        fft_input[i * 2] = detrended_data[i] * window[i]; // Real part
        fft_input[i * 2 + 1] = 0.0f;                     // Imaginary part
    }

    // Perform FFT
    dsps_fft2r_fc32(fft_input, FFT_SIZE);
    dsps_bit_rev_fc32(fft_input, FFT_SIZE);
    dsps_cplx2reC_fc32(fft_input, FFT_SIZE);

    // High-pass filtering by setting bins below 20 Hz to zero
    const float bin_width = SAMPLE_RATE / (float)FFT_SIZE; // Frequency per FFT bin
    int cutoff_bin = (int)(20.0f / bin_width); // Find bin corresponding to 20 Hz
    for (int i = 0; i < cutoff_bin; i++) {
        fft_input[i * 2] = 0.0f;     // Real part
        fft_input[i * 2 + 1] = 0.0f; // Imaginary part
    }

    // Find dominant frequency
    max_magnitude = 0.0;
    int max_index = cutoff_bin; // Start search from cutoff_bin
    for (int i = cutoff_bin; i < FFT_SIZE / 2; i++) {
        float magnitude = sqrt(fft_input[i * 2] * fft_input[i * 2] +
                               fft_input[i * 2 + 1] * fft_input[i * 2 + 1]);
        if (magnitude > max_magnitude) {
            max_magnitude = magnitude;
            max_index = i;
        }
    }

    main_frequency = (float)max_index * SAMPLE_RATE / FFT_SIZE;

    #if ENABLE_ADC_FFT_LOGS
        ESP_LOGI(TAG, "Main Frequency: %.2f Hz, Magnitude: %.2f", main_frequency, max_magnitude);
    #endif

    store_frequency(main_frequency);
}

void collect_adc_continuous_data() {
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
    ESP_LOGI(TAG, "ADC started in continuous mode");

    size_t bytes_read = 0;

    while (1) {
        ESP_ERROR_CHECK(adc_continuous_read(adc_handle, (uint8_t *)adc_buffer, sizeof(adc_buffer), (uint32_t *)&bytes_read, portMAX_DELAY));
        if (bytes_read > 0) {
            #if ENABLE_ADC_FFT_LOGS
                ESP_LOGI(TAG, "Collected %d bytes of ADC data", bytes_read);
            #endif

            // Store data in the ring buffer
            memcpy(collected_data[current_buffer_index], adc_buffer, sizeof(adc_buffer));
            current_buffer_index = (current_buffer_index + 1) % NUM_BUFFERS;

            // Perform FFT
            perform_fft();
        }
        vTaskDelay(pdMS_TO_TICKS(ADC_TASK_DELAY_MS));
    }
}

// New function: save_adc_data
void save_adc_data() {
    ESP_LOGI("ADC_FFT", "Saving ADC buffers...");

    if (adc_semaphore == NULL) {
        adc_semaphore = xSemaphoreCreateBinary();
        if (adc_semaphore == NULL) {
            ESP_LOGE("ADC_FFT", "Failed to create ADC semaphore");
            return;
        }
        xSemaphoreGive(adc_semaphore); // Initialize semaphore
    }

    if (xSemaphoreTake(adc_semaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
        for (int i = 0; i < NUM_BUFFERS; i++) {
            size_t bytes_read = 0;
            ESP_ERROR_CHECK(adc_continuous_read(adc_handle, 
                                               (uint8_t *)collected_data[i], 
                                               sizeof(collected_data[i]), 
                                               (uint32_t *)&bytes_read, 
                                               portMAX_DELAY));
            ESP_LOGI("ADC_FFT", "Buffer %d/%d saved", i + 1, NUM_BUFFERS);
        }
        xSemaphoreGive(adc_semaphore);
        ESP_LOGI("ADC_FFT", "All buffers saved.");
    } else {
        ESP_LOGE("ADC_FFT", "Failed to acquire ADC semaphore");
    }
}
