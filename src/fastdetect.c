// fastdetect.c

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/param.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "config.h"

#include "fastdetect.h"

// -------------------------------------
// Rolling storage of frequency measurements
#define FREQ_STORAGE_SIZE 64
static float s_freqStorage[FREQ_STORAGE_SIZE];
static int   s_freqWritePos = 0;
static int   s_freqCount    = 0;

static const char *TAG = "FASTDETECT";

void store_frequency(float freq)
{
    s_freqStorage[s_freqWritePos] = freq;
    s_freqWritePos = (s_freqWritePos + 1) % FREQ_STORAGE_SIZE;
    if (s_freqCount < FREQ_STORAGE_SIZE) {
        s_freqCount++;
    }
}

static float get_recent_freq(int i)
{
    if (i < 0 || i >= s_freqCount) {
        return 0.0f;
    }
    int idx = s_freqWritePos - 1 - i;
    if (idx < 0) {
        idx += FREQ_STORAGE_SIZE;
    }
    return s_freqStorage[idx];
}

// -------------------------------------
// many-chunk ring
static float s_chunkFreq[NUM_CHUNKS];
static char  s_chunkTrend[NUM_CHUNKS][8]; // "rise", "fall", or "same"

// Compare newVal vs oldVal => "rise", "fall", "same" (with small epsilon)
static const char* get_trend_str(float newVal, float oldVal)
{
    float diff = newVal - oldVal;
    if (fabsf(diff) < 0.001f) {
        return "same";
    }
    return (diff > 0) ? "rise" : "fall";
}

// Builds JSON: {"chunks":[{"freq":..., "trend":"..."}, ...]}
static void build_chunk_json(char *outbuf, size_t outsize)
{
    int written = snprintf(outbuf, outsize, "{\"chunks\":[");
    if (written < 0 || written >= outsize) {
        ESP_LOGE(TAG, "JSON buffer too small at start.");
        return;
    }

    size_t offset = written;

    for (int i = 0; i < NUM_CHUNKS; i++) {
        float freq = s_chunkFreq[i];
        const char *trend = s_chunkTrend[i];
        char entry[64];
        int entry_len = snprintf(entry, sizeof(entry),
                                 "{\"freq\":%.2f,\"trend\":\"%s\"}%s",
                                 freq, trend, (i < NUM_CHUNKS - 1) ? "," : "");

        if (entry_len < 0 || (offset + entry_len) >= outsize - 2) { // Reserve space for closing
            ESP_LOGE(TAG, "JSON buffer too small when adding chunks.");
            break;
        }

        strcat(outbuf, entry);
        offset += entry_len;
    }

    // Append closing brackets
    if ((offset + 2) < outsize) {
        strcat(outbuf, "]}");
    } else {
        ESP_LOGE(TAG, "JSON buffer too small to close JSON.");
    }
}

// -------------------------------------
// The 100 ms background task
// Each loop, we gather up to 4 newest freq measurements => new chunk
// SHIFT old chunk data, place new chunk at index 0
static void fast_detect_task(void *arg)
{
    // Initialize chunk ring
    for (int i = 0; i < NUM_CHUNKS; i++) {
        s_chunkFreq[i] = 0.0f;
        strcpy(s_chunkTrend[i], "same");
    }

    while (1) {
        // 100 ms updates
        vTaskDelay(pdMS_TO_TICKS(100));

        // Gather up to 4 newest freq measurements
        float sum = 0.0f;
        int   count = 0;
        for (int i = 0; i < 4; i++) {
            float f = get_recent_freq(i);
            if (f != 0.0f) {
                sum += f;
                count++;
            }
        }
        if (count == 0) {
            ESP_LOGI(TAG, "No freq data yet, skipping chunk update...");
            continue;
        }

        float newAvg = sum / count;

        // Shift old chunks
        for (int i = NUM_CHUNKS - 1; i > 0; i--) {
            s_chunkFreq[i] = s_chunkFreq[i - 1];
            strcpy(s_chunkTrend[i], s_chunkTrend[i - 1]);
        }

        float oldVal = s_chunkFreq[1];
        s_chunkFreq[0] = newAvg;
        const char* trend = get_trend_str(newAvg, oldVal);
        strcpy(s_chunkTrend[0], trend);

        ESP_LOGI(TAG, "Chunk=%.2f => %s vs %.2f", newAvg, trend, oldVal);

        // Data is pushed via WebSocket upon client request
    }
}

void init_fastdetect_task(void)
{
    xTaskCreate(fast_detect_task, "fast_detect_task", 4096, NULL, 5, NULL);
}

// -------------------------------------
// WebSocket handler
esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        // This is the handshake
        ESP_LOGI(TAG, "New WS client connected");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // 1) Get the frame length
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ws_handler: get length error: %d", ret);
        return ret;
    }

    if (ws_pkt.len) {
        uint8_t *buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
        if (!buf) return ESP_ERR_NO_MEM;

        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "WS got: %s", ws_pkt.payload);

            // If we see "getdata", build the chunk JSON & respond
            if (strcmp((char*)ws_pkt.payload, "getdata") == 0) {
                char json[JSON_BUFFER_SIZE];  // Use the updated buffer size
                memset(json, 0, sizeof(json));
                build_chunk_json(json, sizeof(json));

                ESP_LOGI(TAG, "Sending JSON: %s", json); // Debug log
                httpd_ws_frame_t resp;
                memset(&resp, 0, sizeof(resp));
                resp.type    = HTTPD_WS_TYPE_TEXT;
                resp.payload = (uint8_t*)json;
                resp.len     = strlen(json);

                esp_err_t r2 = httpd_ws_send_frame(req, &resp);
                if (r2 != ESP_OK) {
                    ESP_LOGW(TAG, "Send chunk JSON failed: %d", r2);
                }
            }
        } else {
            ESP_LOGE(TAG, "ws_handler: data recv error: %d", ret);
        }
        free(buf);
    }

    return ESP_OK;
}

// -------------------------------------
// Serve /fastdetect from SPIFFS
#define FASTDETECT_HTML_PATH  "/spiffs/fastdetect.html"

esp_err_t fastdetect_handler(httpd_req_t *req)
{
    FILE *f = fopen(FASTDETECT_HTML_PATH, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", FASTDETECT_HTML_PATH);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "HTML file not found");
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ftell failed");
        return ESP_FAIL;
    }

    char *buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    fread(buf, 1, size, f);
    fclose(f);
    buf[size] = '\0';

    httpd_resp_set_type(req, "text/html");
    esp_err_t res = httpd_resp_send(req, buf, size);

    free(buf);
    return res;
}
