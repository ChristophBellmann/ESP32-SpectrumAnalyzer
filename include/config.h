// config.h

#ifndef CONFIG_H
#define CONFIG_H

// ---------------------
// Logging Configuration
// ---------------------
#define ENABLE_FASTDETECT_LOGS 0  // Set to 1 to enable logs for fastdetect, 0 to disable
#define ENABLE_ADC_FFT_LOGS 0     // Set to 1 to enable logs for adc_fft, 0 to disable

// ---------------------
// Audio and FFT Configuration
// ---------------------
#define SAMPLE_RATE 44100          // Sample rate in Hz
#define FFT_SIZE 1024              // FFT size (number of samples to collect)
#define ADC_CHANNEL ADC_CHANNEL_0  // ADC channel
#define ADC_ATTEN ADC_ATTEN_DB_12  // ADC attenuation (0-3.3V range)

// ---------------------
// Wi-Fi Configuration
// ---------------------
#define WIFI_SSID "Pixel_cb"        // Wi-Fi SSID
#define WIFI_PASS "Lassmichrein!"   // Wi-Fi Password

// ---------------------
// Buffer and Task Configuration
// ---------------------
#define NUM_BUFFERS 60             // Number of buffers in the ring buffer
#define ADC_TASK_DELAY_MS 100      // ADC task delay in milliseconds

// ---------------------
// Frequency and JSON Configuration
// ---------------------
#define FREQ_STORAGE_SIZE 64       // Size of the frequency storage buffer
#define NUM_CHUNKS 20              // Number of chunks for trend analysis
#define JSON_BUFFER_SIZE 1024       // Buffer size for JSON data (1536)

#endif // CONFIG_H
