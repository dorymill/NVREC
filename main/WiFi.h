#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>
#include "esp_err.h"

/* Function prototypes */
esp_err_t wifiApInit (const char* ssid, const char* password, uint8_t channel);


#endif 