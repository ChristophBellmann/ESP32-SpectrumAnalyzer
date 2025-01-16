#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"
#include "esp_http_server.h"

// Initialisiert Wi-Fi im STA-Modus
void wifi_init_sta();

// Startet den Webserver
httpd_handle_t start_webserver();

#endif // WIFI_H
