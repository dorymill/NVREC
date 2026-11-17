#ifndef STREAM_H
#define STREAM_H

#include "esp_err.h"

/* Function Prototypes */
esp_err_t streamInit (void);
void streamPublishFrame (const uint8_t* frame, size_t frameLen);

#endif