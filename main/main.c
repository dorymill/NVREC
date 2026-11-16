#include <stdio.h>
#include "Types.h"
#include "Consts.h"
#include "Camera.h"
#include "Encoder.h"
#include "esp_log.h"

static const char* TAG = "|Main|";

void app_main(void)
{

    Camera_t cam;
    Encoder_t h264;
    uint32_t testFrameCnt = 3000;

    ESP_ERROR_CHECK(cameraInit(&cam, WIDTH, HEIGHT, PX_FMT));
    ESP_ERROR_CHECK(cameraStart(&cam));
    ESP_ERROR_CHECK(encoderInit(&h264, SD, WIDTH, HEIGHT));

    ESP_LOGI(TAG, "[+] Recording %d frames. . .", testFrameCnt);

    for(int idx = 0; idx < testFrameCnt; idx++) {

        void*  frame;
        size_t fLen;
        int    fIdx;

        /* Acquire the frame */
        cameraGetFrame(&cam, &frame, &fLen, &fIdx);

        /*Encode and save block */

        /* H.264 Encoding */
        uint8_t* h264data;
        size_t h264len;

        encoderEncode(&h264, frame, fLen, &h264data, &h264len);

        /* Do something with the data (add to queue for a task.) */


        /* Release the frame */
        cameraReleaseFrame(&cam, fIdx);

    }

    ESP_ERROR_CHECK(encoderDeInit(&h264));
    ESP_ERROR_CHECK(cameraStop(&cam));
    ESP_ERROR_CHECK(cameraDeInit(&cam));

    ESP_LOGI(TAG, "[+] Done!");
}
