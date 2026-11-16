#include "Encoder.h"
#include "Consts.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_video_init.h"
#include "esp_video_device.h"
#include "linux/videodev2.h"

static const char* TAG = "|Encoder|";

/* Function implementations */

/**
 * @brief This function sets the encoder device path and codec format
 *        as determined by the two pre-selected paths: MP4 for SD-Card
 *        file I/O and JPEG for real-time streaming via WiFi.
 * 
 * @param dest       Streaming Destionation: SD, WIFI
 * @param devPath    Linux device path to streaming device (See esp_video_Device.h)
 * @param codecFmt   Codec Pixel Format
 * @return esp_err_t Error Code
 */
static esp_err_t getDeviceParams (EncDest_t dest, const char** devPath, uint32_t* codecFmt)
{

    switch(dest) {


        case SD: {
            *devPath  = H264_DEV;
            *codecFmt = V4L2_PIX_FMT_H264;
            return ESP_OK;
        };

        case WIFI: {
            *devPath  = JPEG_DEV;
            *codecFmt = V4L2_PIX_FMT_JPEG;
            return ESP_OK;
        };

        default: {
            return ESP_ERR_INVALID_ARG;
        };
    }
}

/**
 * @brief 
 * 
 * @param enc 
 * @param dest 
 * @param width 
 * @param height 
 * @return esp_err_t 
 */
esp_err_t encoderInit(Encoder_t* enc, EncDest_t dest, uint32_t width, 
                      uint32_t height)
{

    ESP_LOGI(TAG, "[+] Encoder starting. . .");

    /* Internal variables */
    const char* devPath;
    uint32_t    codecFmt;
    int         type;
    struct v4l2_format         format = {0};
    struct v4l2_requestbuffers req    = {0};
    struct v4l2_buffer         buf    = {0};

    /* Set the encoder to zeroes */
    memset(enc, 0, sizeof(*enc));
    enc->dest = dest;

    /* Get the device parameters for it */
    ESP_RETURN_ON_ERROR(getDeviceParams(dest, &devPath, &codecFmt), TAG, "[-] Unknown encoder type selected.");

    /* Open the Encoder device */
    enc->fd = open(devPath, O_RDWR);
    ESP_RETURN_ON_FALSE(enc->fd >= 0, ESP_FAIL, TAG, "[-] Failed to open encoder %s", devPath);

    /* Setup the output format (YUV420 Raw into the encoder) */
    format.type                = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width       = width;
    format.fmt.pix.height      = height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;

    /* Set the encoder input format with an ioctl(VIDIOC_S_FMT) */
    if(ioctl(enc->fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "[-] Failed to set encoder input format.");
        close(enc->fd);
        return ESP_FAIL;
    }

    /* Set up the input buffer using user pointers in application space */
    req.count  = 1;
    req.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_USERPTR; /* Zero copy! */

    if(ioctl(enc->fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "[-] Failed to set request encoder output buffers.");
        close(enc->fd);
        return ESP_FAIL;
    }

    /* Set the H.264 Bit Rate and Quality Controls for the SD Path */
    if(dest == SD) {

        struct v4l2_ext_controls controls;
        struct v4l2_ext_control  control[1];

        /* Init the controls and control structures */
        controls.ctrl_class = V4L2_CTRL_CLASS_CODEC;
        controls.count      = 1;
        controls.controls   = control;

        /* Set the bitrate */
        control[0].id    = V4L2_CID_MPEG_VIDEO_BITRATE;
        control[0].value = SD_BITRATE; /* 2 Mbps, tuneable */
        if(ioctl(enc->fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "[-] Failed to set SD Output bitrate.");
            return ESP_FAIL;
        }

        /* Set the keyframe rate */
        control[0].id     = V4L2_CID_MPEG_VIDEO_H264_I_PERIOD;
        control[0].value = SD_KEYFRAME_RATE;
        if(ioctl(enc->fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "[-] Failed to SD Output keyframe rate.");
            return ESP_FAIL;
        }
    }

    /* Set up the output encoder format data */
    memset(&format, 0, sizeof(format));
    format.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width       = WIDTH;
    format.fmt.pix.height      = HEIGHT;
    format.fmt.pix.pixelformat = codecFmt;

    if(ioctl(enc->fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "[-] Failed to set encoder output format!");
        close(enc->fd);
        return ESP_FAIL;
    }

    /* Set up the output buffers */
    req.count  = 1;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if(ioctl(enc->fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "[-] Failed to request encoder output buffer.");
        close(enc->fd);
        return ESP_FAIL;
    }
    
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = 0;
    if(ioctl(enc->fd, VIDIOC_QUERYBUF, &buf) != 0) {
        ESP_LOGE(TAG, "[-] Failed to query encoder output buffer.");
        close(enc->fd);
        return ESP_FAIL;
    }

    /* Connect the encoder structure buffer details to device mmap'd buffers */
    enc->outBuffs = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, enc->fd, buf.m.offset);
    ESP_RETURN_ON_FALSE(enc->outBuffs != MAP_FAILED, ESP_FAIL, TAG, "[-] Failed to mmap encoder output buffer.");
    enc->outSize = buf.length;

    /* Queue up the encoder output buffer with the device! */
    if(ioctl(enc->fd, VIDIOC_QBUF, &buf) !=0) {
        ESP_LOGE(TAG,"[-] Failed to queue encoder output buffers.");
        return ESP_FAIL;
    }

    /* Finally start the streams! */
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_RETURN_ON_FALSE(ioctl(enc->fd, VIDIOC_STREAMON, &type) == 0, ESP_FAIL, TAG, "[-] Failed to start encoder capture stream.");
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ESP_RETURN_ON_FALSE(ioctl(enc->fd, VIDIOC_STREAMON, &type) == 0, ESP_FAIL, TAG, "[-] Failed to start encoder output stream.");

    ESP_LOGI(TAG, "[+] Encoder online!");

    return ESP_OK;
}

esp_err_t encoderEncode(Encoder_t* enc, void* yuvFrame,
                        size_t yuvLen, uint8_t** output, size_t* outLen)
{

    static unsigned long statusCntr = 0;

    /* Input and output buffers */
    struct v4l2_buffer outBuff = {0};
    struct v4l2_buffer inBuff  = {0};

    /* Feed the raw frame to the encoder */
    outBuff.type      = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    outBuff.memory    = V4L2_MEMORY_USERPTR;
    outBuff.index     = 0;
    outBuff.m.userptr = (unsigned long) yuvFrame;
    outBuff.length    = yuvLen;

    if(ioctl(enc->fd, VIDIOC_QBUF, &outBuff) != 0) {
        ESP_LOGE(TAG, "[-] Failed to queue raw from for encoding.");
        return ESP_FAIL;
    }

    /* Get the output, and put it in the output for the function call */
    inBuff.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    inBuff.memory = V4L2_MEMORY_MMAP;

    if(ioctl(enc->fd, VIDIOC_DQBUF, &inBuff) != 0) {
        ESP_LOGE(TAG, "[-] Failed to dequeue encoded frame.");
        return ESP_FAIL;
    }

    *output = enc->outBuffs;
    *outLen = inBuff.bytesused;

    /* Reclaime the output frame buffer back to the queue. */
    if(ioctl(enc->fd, VIDIOC_DQBUF, &outBuff) != 0) {
        ESP_LOGE(TAG, "[-] Failed to dequeue spent raw frame.");
        return ESP_FAIL;
    }

    /* Requeue input frame back to the queue */
    if(ioctl(enc->fd, VIDIOC_QBUF, &inBuff) != 0) {
        ESP_LOGE(TAG, "[-] Failed to requeue capture buffer.");
        return ESP_FAIL;
    }

    statusCntr++;

    if(statusCntr % ENC_REPORT_RATE == 0) {
        ESP_LOGI(TAG, "[+] Encoded %ld frames.", statusCntr);
    }

    return ESP_OK;
}


esp_err_t encoderDeInit (Encoder_t* enc)
{

    /* Turn off the streams */
    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ioctl(enc->fd, VIDIOC_STREAMOFF, &type);
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(enc->fd, VIDIOC_STREAMOFF, &type);

    /* Unmap the buffers */
    if(enc->outBuffs) {
        munmap(enc->outBuffs, enc->outSize);
    }

    /* Close the device */
    close(enc->fd);

    ESP_LOGI(TAG, "[+] Encoder de-initialized.");

    return ESP_OK;
}