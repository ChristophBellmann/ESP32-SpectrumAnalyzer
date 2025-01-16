#ifndef ADC_FFT_H
#define ADC_FFT_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc/adc_continuous.h"

// ADC-Handle extern deklarieren
extern adc_continuous_handle_t adc_handle;

// Globale Variablen für Hauptfrequenz und Magnitude
extern float main_frequency; // Hauptfrequenz
extern float max_magnitude;  // Magnitude

// Initialisiert den ADC im kontinuierlichen Modus
void configure_adc_continuous();

// Startet den Task, der ADC-Daten liest und FFT berechnet
void collect_adc_continuous_data();

#endif // ADC_FFT_H
