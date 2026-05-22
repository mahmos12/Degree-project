#include "thingspeak.h"
#include "secrets.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_client.h"

static const char *TAG = "THINGSPEAK";
static EventGroupHandle_t wifi_event_group;

// WiFi connection status bit
#define WIFI_CONNECTED_BIT BIT0

// Handle WiFi and IP events
static void wifi_event_handler( void *arg, esp_event_base_t event_base, int32_t event_id,void *event_data){

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){

        // Connect when station starts
        esp_wifi_connect();
    }else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){

        // Reconnect if connection is lost
        esp_wifi_connect();
    }else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){

        // Signal successful connection
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initialize WiFi connection
void thingspeak_init(void){
    nvs_flash_init();

    wifi_event_group = xEventGroupCreate();

    esp_netif_init();

    esp_event_loop_create_default();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);

    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    // Configure WiFi credentials
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD
        }
    };

    esp_wifi_set_mode(WIFI_MODE_STA);

    esp_wifi_set_config( WIFI_IF_STA, &wifi_config);

    esp_wifi_start();

    // Wait until connected
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    ESP_LOGI(TAG, "WiFi connected");
}

// Send sensor data to ThingSpeak
void thingspeak_send(int bpm, int motion, int finger)
{
    char url[256];

    // Build ThingSpeak update request
    sprintf(url, "http://api.thingspeak.com/update?api_key=%s&field1=%d&field2=%d&field3=%d", 
        THINGSPEAK_API_KEY, bpm, motion, finger);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK){
        ESP_LOGI(TAG, "Data sent");
    } else{
        ESP_LOGE(TAG, "Send failed");
    }

    esp_http_client_cleanup(client);
}