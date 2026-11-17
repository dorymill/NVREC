#include <stdio.h>
#include "Types.h"
#include "esp_log.h"
#include "Consts.h"
#include "Camera.h"
#include "Encoder.h"
#include "WiFi.h"
#include "Stream.h"

static const char* TAG = "|Main|";

void app_main(void)
{

    Camera_t cam;
    Encoder_t jpegEnc;
    uint32_t testFrameCnt = 3000;

    ESP_ERROR_CHECK(cameraInit(&cam, WIDTH, HEIGHT, PX_FMT));
    ESP_ERROR_CHECK(cameraStart(&cam));
    ESP_ERROR_CHECK(encoderInit(&jpegEnc, WIFI, WIDTH, HEIGHT));
    ESP_ERROR_CHECK(wifiApInit(AP_SSID, AP_PASS, 8));
    ESP_ERROR_CHECK(streamInit());

    ESP_LOGI(TAG, "[+] Main Test Loop Running. . .", testFrameCnt);

    for(;;) {

        void*  frame;
        size_t fLen;
        int    fIdx;

        /* Acquire the frame */
        cameraGetFrame(&cam, &frame, &fLen, &fIdx);

        /*Encode and save block */

        /* H.264 Encoding */
        uint8_t* jpegData;
        size_t jpegLen;

        encoderEncode(&jpegEnc, frame, fLen, &jpegData, &jpegLen);

        /* Do something with the data (add to queue for a task.) */
        streamPublishFrame(jpegData, jpegLen);

        /* Release the frame */
        cameraReleaseFrame(&cam, fIdx);

    }

    ESP_ERROR_CHECK(encoderDeInit(&jpegEnc));
    ESP_ERROR_CHECK(cameraStop(&cam));
    ESP_ERROR_CHECK(cameraDeInit(&cam));

    ESP_LOGI(TAG, "[+] Done!");
}
