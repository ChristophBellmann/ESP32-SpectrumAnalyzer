#ifndef ADC_FFT_H
#define ADC_FFT_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc/adc_continuous.h"
#include "config.h" // Include config for NUM_BUFFERS and FFT_SIZE

// ADC handle for continuous reading
extern adc_continuous_handle_t adc_handle;

// Global variables for main frequency and magnitude
extern float main_frequency;
extern float max_magnitude;

// Shared ring buffer and current buffer index
extern int16_t collected_data[NUM_BUFFERS][FFT_SIZE];
extern int current_buffer_index; // Current buffer index in the ring buffer

// Initializes the ADC in continuous mode
void configure_adc_continuous();

// Starts the task for reading ADC data and performing FFT
void collect_adc_continuous_data();

// Saves data to the shared ring buffer
void save_adc_data(); // Add this function declaration

#endif // ADC_FFT_H
