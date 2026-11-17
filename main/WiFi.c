#include "WiFi.h"

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_wifi_remote.h"
#include "esp_wifi_default.h"
#include "esp_hosted.h"
#include "Consts.h"

static const char* TAG = "|WiFi|";

/**
 * @brief 
 * 
 * @param ssid 
 * @param password 
 * @param channel 
 * @return esp_err_t 
 */
esp_err_t wifiApInit (const char* ssid, const char* password, uint8_t channel)
{

    ESP_LOGI(TAG, "[+] Starting Wi-Fi AP. . .");

    /* Clear out Non-Volatile Storage for Wi-Fi Cal Data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "[-] Failed to erase NVS.");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "[-] Failed to initialize NVS.");

    /* Initialize the Net Interface and Event Loop */
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "[-] Failed to initialize Net Interface.");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "[-] Failed to create event loop.");

    /* Start the link to the ESP32-C6 */
    ESP_RETURN_ON_FALSE(esp_hosted_connect_to_slave() == 0, ESP_FAIL, TAG, "[-] Failed to connect to Wi-Fi coprocessor.");

    /* Create teh default access point */
    ESP_RETURN_ON_FALSE(esp_netif_create_default_wifi_ap() != NULL, ESP_FAIL, TAG, "[-] Failed to create AP.");

    /* Set up the initial configuration structure */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "[-] Failed to initialize AP.");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "[-] Failed to set AP mode.");

    /* Set up the user provided configuration structure */
    wifi_config_t apCfg = {

        .ap = {
            .channel        = channel,
            .max_connection = MAX_CONNECTIONS,
            .authmode       = password ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
            
        },

    };

    /* Copy the SSID and Password Strings into the config structure */
    strncpy((char*) apCfg.ap.ssid, ssid, sizeof(apCfg.ap.ssid) - 1);

    if (password) {
        strncpy((char*) apCfg.ap.password, password, sizeof(apCfg.ap.password) -1);
    }

    /* Set the new settings and launch the AP */
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &apCfg), TAG, "[-] Failed to configure AP.");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "[-] Failed to start AP.");

    ESP_LOGI(TAG, "[+] Wifi Online! SSID: %s | Password: %s", apCfg.ap.ssid, apCfg.ap.password);

    return ESP_OK;
}