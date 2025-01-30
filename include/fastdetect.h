#ifndef FASTDETECT_H
#define FASTDETECT_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

void store_frequency(float freq);
void init_fastdetect_task(void);

// HTML handler for /fastdetect
esp_err_t fastdetect_handler(httpd_req_t *req);

// The WebSocket handler is declared here if you like:
esp_err_t ws_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

#endif // FASTDETECT_H
