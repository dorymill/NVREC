#ifndef CAMERA_H
#define CAMERA_H

#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_video_init.h"
#include "esp_video_device.h"
#include "linux/videodev2.h"
#include "Consts.h"


/* Camera config structures */
/* MIPI-CSI Configuration */
static const esp_video_init_csi_config_t s_csiConfig = {

    .sccb_config = {

        .init_sccb = true,
        .i2c_config = {

            .port = 0,
            .scl_pin = CAM_I2C_SCL_PIN,
            .sda_pin = CAM_I2C_SDA_PIN,

        },

        .freq = 400000,

    },

    .reset_pin = -1,
    .pwdn_pin  = -1,

};

/* High Level Configuration Structure */
static const esp_video_init_config_t s_camConfig = {
    .csi = &s_csiConfig,
};

/* User facing Camera Structure */
typedef struct {

    int fd;
    void* buffer[CAM_BUF_SIZE];
    size_t bufferSize;
    uint32_t width, height, pixelFormat;

} Camera_t;

/* Camera Function Prototypes */
esp_err_t cameraInit (Camera_t* cam, uint32_t width, uint32_t height, uint32_t pixelFormat);
esp_err_t cameraStart (Camera_t* cam);
esp_err_t cameraGetFrame (Camera_t* cam, void** output, size_t* outLen, int* outIdx);
esp_err_t cameraReleaseFrame (Camera_t* cam, int idx);
esp_err_t cameraStop (Camera_t* cam);
esp_err_t cameraDeInit (Camera_t* cam);


#endif