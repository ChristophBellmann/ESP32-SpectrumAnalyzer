#include "wifi.h"
#include "adc_fft.h"
#include "wav.h"

void app_main() {
    wifi_init_sta();
    configure_adc_continuous();
    start_webserver();
    xTaskCreate(collect_adc_continuous_data, "ADC_Task", 4096, NULL, 5, NULL);
}
