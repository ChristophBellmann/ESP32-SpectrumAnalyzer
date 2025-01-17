#include "wifi.h"
#include "adc_fft.h"
#include "wav.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "FreeRAM";

// Task to monitor and log the free RAM periodically
void monitor_free_ram_task(void *param) {
    while (1) {
        ESP_LOGI(TAG, "Free heap size: %u bytes", (unsigned int)esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(1000)); // Log every 1 second
    }
}

void app_main() {
    wifi_init_sta();
    configure_adc_continuous();
    start_webserver();
    xTaskCreate(collect_adc_continuous_data, "ADC_Task", 4096, NULL, 5, NULL);

    // Create a task to monitor free RAM
    xTaskCreate(monitor_free_ram_task, "Monitor_FreeRAM_Task", 2048, NULL, 1, NULL);
}
