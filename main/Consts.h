#ifndef CONSTS_H
#define CONSTS_H

/* Camera related defines */
#define CAM_BUF_SIZE  2    /* V4L2 Capture Buffers */

#define CAM_I2C_SCL_PIN 8  /* I2C Communication Pins */
#define CAM_I2C_SDA_PIN 7

#define HEIGHT 1080
#define WIDTH  1920
#define PX_FMT V4L2_PIX_FMT_YUV420

/* Encoder Defines */
#define H264_DEV         ESP_VIDEO_H264_DEVICE_NAME
#define JPEG_DEV         ESP_VIDEO_JPEG_DEVICE_NAME
#define SD_BITRATE       2000000                     /* 2 Mbps, tunable */
#define SD_KEYFRAME_RATE 30                          /* Keyframe Rate [frames]*/
#define ENC_REPORT_RATE  300                         /* Report rate for debug logging */

/* WiFi Defines */
#define MAX_CONNECTIONS 1
#define AP_SSID "NVREC Mk. 0"
#define AP_PASS "nvtest365"

/* Stream defines */
#define STREAM_WAIT_MS 11
#define HEADER_LEN     64

#endif