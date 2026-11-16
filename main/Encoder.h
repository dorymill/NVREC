#ifndef ENCODER_H
#define ENCODER_H

#include "Consts.h"
#include "esp_err.h"

/* Encoder structures */
typedef enum {

    SD,
    WIFI

} EncDest_t;

typedef struct {

    int      fd;
    uint8_t* outBuffs;
    size_t   outSize;
    EncDest_t dest;

} Encoder_t;


/* Encoder Function Prototypes */
esp_err_t encoderInit(Encoder_t* enc, EncDest_t dest, uint32_t width, 
                      uint32_t height);
esp_err_t encoderDeInit(Encoder_t* enc);
esp_err_t encoderEncode(Encoder_t* enc, void* yuvFrame,
                        size_t yuvLen, uint8_t** output, size_t* outLen);

#endif