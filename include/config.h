#ifndef CONFIG_H
#define CONFIG_H

#define SAMPLE_RATE 44100          // Sample rate in Hz
#define FFT_SIZE 1024              // FFT size (number of samples to collect)
#define ADC_CHANNEL ADC_CHANNEL_0  // ADC channel
#define ADC_ATTEN ADC_ATTEN_DB_12  // ADC attenuation (0-3.3V range)
#define WIFI_SSID "Pixel_cb"
#define WIFI_PASS "Lassmichrein!"
#define NUM_BUFFERS 60             // Number of buffers in the ring buffer
#define ADC_TASK_DELAY_MS 100

#endif // CONFIG_H
