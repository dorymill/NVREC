#include "Stream.h"

#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "Consts.h"

static const char* TAG = "|Stream|";

#define BOUNDARY "frame"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" BOUNDARY;
static const char* STREAM_BOUNDARY     = "\r\n--" BOUNDARY "\r\n";
static const char* STREAM_PART_HEADER  = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static uint8_t* s_frame     = NULL;
static size_t   s_frameLen  = 0;
static size_t   s_frameCap  = 0;

static SemaphoreHandle_t s_lock;

static esp_err_t streamHandler (httpd_req_t* req)
{
    /* Set the response up for streaming content */
    httpd_resp_set_type(req, STREAM_CONTENT_TYPE);

    while(true) {

        /* Use the semaphore to lock the frame */
        xSemaphoreTake(s_lock, portMAX_DELAY);

        /* Copy it */
        size_t len = s_frameLen;
        uint8_t* copy = len ? (uint8_t *) heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
        if(copy) memcpy(copy, s_frame, len);
        
        if(!copy) {
            xSemaphoreGive(s_lock);
            continue;
        }

        /* Release the semaphore when we've copied the frame */
        xSemaphoreGive(s_lock);

        /* Craft the response header */
        char header[64];
        int headerLen = snprintf(header, sizeof(header), STREAM_PART_HEADER, (unsigned) len);

        /* Send the stream boundary, the header, then the data itself and release the copy */
        esp_err_t ret = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (ret == ESP_OK) ret = httpd_resp_send_chunk(req, header, headerLen);
        if (ret == ESP_OK) ret = httpd_resp_send_chunk(req, (char*) copy, len);
        free(copy);

        /* If the client disconnected, break. */
        if (ret != ESP_OK) break;

        /* Frame Rate Capping at ~3x output */
    }

    return ESP_OK;
}

esp_err_t streamInit (void)
{
    
    ESP_LOGI(TAG, "[+] Initializing stream. . .");

    /* Create a mutex for frame access */
    s_lock = xSemaphoreCreateMutex();

    /* Configure the HTTP server parameters */
    httpd_config_t cfg    = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    esp_err_t ret = httpd_start(&server, &cfg);

    if(ret != ESP_OK) {
        return ret;
    }

    httpd_uri_t streamUri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = streamHandler,
    };

    /* Fire! */
    ret = httpd_register_uri_handler(server, &streamUri);

    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "[-] Failed to start stream.");
    } else {
        ESP_LOGI(TAG, "[+] Stream initialized!");
    }

    return ret;
}

void streamPublishFrame (const uint8_t* frame, size_t frameLen)
{
    /* Lock the static frame */
    xSemaphoreTake(s_lock, portMAX_DELAY);

    /* Dynamically track the max frame length */
    if(frameLen > s_frameCap) {
        s_frame    = heap_caps_realloc(s_frame, frameLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        s_frameCap = frameLen;
    };

    /* Copy the frame to the internal, static frame */
    memcpy(s_frame, frame, frameLen);
    s_frameLen = frameLen;

    /* Release the Semaphore */
    xSemaphoreGive(s_lock);
}