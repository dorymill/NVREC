#include "Camera.h"
#include "Consts.h"
#include "Camera.h"

#include <sys/ioctl.h>

static const char* TAG = "|Camera|";

/* Function implementations */

/**
 * @brief This function initializes the camera device, returning
 *        an error if it fails ay any point. The general procedure
 *        is to set the camera object setttings, open the file 
 *        descriptor to it, set the format and buffer parameters
 *        initialize and request the buffers, then we're ready to
 *        rock and roll! Ioctl calls are the primary driver of this
 *        process.
 * 
 * @param cam         Pointer to camera device
 * @return esp_err_t  Error Code
 */
esp_err_t cameraInit (Camera_t* cam, uint32_t width, uint32_t height, uint32_t pixelFormat)
{

    ESP_LOGI(TAG, "[+] Initializing camera. . .");

    struct v4l2_format format      = {0};
    struct v4l2_requestbuffers req = {0};

    memset(cam, 0, sizeof(*cam));

    /* Set the camera params */
    cam->width       = width;
    cam->height      = height;
    cam->pixelFormat = pixelFormat;

    /* Bring up the hardware and probe the sensor */
    ESP_RETURN_ON_ERROR(esp_video_init(&s_camConfig), TAG, "[-] Failed to initialize camera!\n");

    /* Open a file descriptor to the device */
    cam->fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDONLY);
    ESP_RETURN_ON_FALSE(cam->fd >= 0, ESP_FAIL, TAG, "[-] Failed to open %s\n", ESP_VIDEO_MIPI_CSI_DEVICE_NAME);

    /* Setup format parameters */
    format.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width       = cam->width;
    format.fmt.pix.height      = cam->height;
    format.fmt.pix.pixelformat = cam->pixelFormat;

    /* Set request buffer parameters and buffering */
    req.count  = CAM_BUF_SIZE;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(cam->fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "[-] Failed to set format!\n");
        close(cam->fd);
        return ESP_FAIL;
    }

    if(ioctl(cam->fd, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "[-] Failed to request buffers.\n");
        close(cam->fd);
        return ESP_FAIL;
    }

    for(int idx = 0; idx < CAM_BUF_SIZE; idx++) {

        struct v4l2_buffer buf = {0};
        
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = idx;

        if(ioctl(cam->fd, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "[-] Failed to query buffer!\n");
            return ESP_FAIL;
        }

        cam->buffer[idx] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, cam->fd, buf.m.offset);
        ESP_RETURN_ON_FALSE(cam->buffer[idx] != MAP_FAILED, ESP_FAIL, TAG, "[-] Failed to mmap buffer %d", idx);
        cam->bufferSize = buf.length;

        if(ioctl(cam->fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "[-] Failed to queue buffer %d.\n", idx);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "[+] Camera online!");

    return ESP_OK;
}

/**
 * @brief This function will deinitialize the camera device, freeing
 *        up the memory for the device, and closing the file descriptor.
 * 
 * @param cam         Pointer to camera device
 * @return esp_err_t  Error Code
 */
esp_err_t cameraDeInit (Camera_t* cam)
{

    /* Iteratively free the buffers. */
    for (int idx = 0; idx < CAM_BUF_SIZE; idx++) {
        if (cam->buffer[idx]) {
            munmap(cam->buffer[idx], cam->bufferSize);
        }
    }

    /* Close the file descriptor and shutdown the HW. */
    close(cam->fd);

    ESP_LOGI(TAG, "[+] Camera de-initialized.");

    return esp_video_deinit();
}

/**
 * @brief This function starts the video streaming with an ioctl
 *        call with the video i/o stream on command.
 * 
 * @param cam         Pointer to camera device
 * @return esp_err_t  Error Code
 */
esp_err_t cameraStart (Camera_t* cam)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ESP_RETURN_ON_FALSE(ioctl(cam->fd, VIDIOC_STREAMON, &type) == 0, ESP_FAIL, TAG, "[-] Failed to start video stream.");
   
    return ESP_OK;
}

/**
 * @brief This function stops the video streaming with an ioctl
 *        call with the video i/o stream off command.
 * 
 * @param cam         Pointer to camera device
 * @return esp_err_t  Error Code
 */
esp_err_t cameraStop (Camera_t* cam)
{
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ESP_RETURN_ON_FALSE(ioctl(cam->fd, VIDIOC_STREAMOFF, &type) == 0, ESP_FAIL, TAG, "[-] Failed to stop video stream.");
   
    return ESP_OK;
}

/**
 * @brief This function is the work function for the 
 *        camera device, acquiring the individual video
 *        frames for passage to further processing, e.g.
 *        encoders and SD Cards.
 * 
 * @param cam        Pointer to camera device
 * @param output     Pointer to output buffer address
 * @param outLen     Output length in bytes
 * @param outIdx     Output Buffer index
 * @return esp_err_t Error COde
 */
esp_err_t cameraGetFrame (Camera_t* cam, void** output, size_t* outLen, int* outIdx)
{

    /* Set up the frame buffer */
    struct v4l2_buffer buf = {0};
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    /* Pull the frame off the HW buffer */
    ESP_RETURN_ON_FALSE(ioctl(cam->fd, VIDIOC_DQBUF, &buf) == 0, ESP_FAIL, TAG, "[-] Failed to get frame.\n");

    /* Set the output data and details pointers */
    *output = cam->buffer[buf.index];
    *outLen = buf.bytesused;
    *outIdx = buf.index;

    return ESP_OK;
}

/**
 * @brief This function must be called after frame handling
 *        and will release the frame to allow further capture.
 * 
 * @param cam        Pointer to camera device
 * @param idx        Buffer index for the frame to release
 * @return esp_err_t 
 */
esp_err_t cameraReleaseFrame (Camera_t* cam, int idx)
{

    /* Set up the frame buffer */
    struct v4l2_buffer buf = {0};
    buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index  = idx;

    /* Release the frame */
    ESP_RETURN_ON_FALSE(ioctl(cam->fd, VIDIOC_QBUF, &buf) == 0, ESP_FAIL, TAG, "[-] Failed to release frame (%d)\n", idx);
    return ESP_OK;
}