#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"


#include "mqtt_client.h"
#include "test_main.h"
#include "wifi_conn.h"

const char *TAG_MQTT = "mqtt_esp";
uint8_t start = 0;

TaskHandle_t mqtt_start_task_handle = NULL;

void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG_MQTT, "Last error %s: 0x%x", message, error_code);
    }
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG_MQTT, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "ota/esp0032", 0);
        ESP_LOGI(TAG_MQTT, "subscribe successful, msg_id=%d", msg_id);
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DATA");

        start = atoi(event->data);
        printf("start = %d\n",start);
        break;
        

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("Reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("Reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("Captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG_MQTT, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    default:
        ESP_LOGI(TAG_MQTT, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void *arg){
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    while (1){  
        if (wifi_connected == 1){
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_mqtt_client_config_t mqtt_cfg = {
                .broker.address.uri = "mqtt://mqtt.eclipseprojects.io",
                .session.last_will.topic = "ota/esp0032"
            };

            esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
            esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
            esp_mqtt_client_start(client);
            vTaskDelay(pdMS_TO_TICKS(100));
            xTaskNotifyGive(trigger_tasks_handle);
            vTaskDelete(NULL);
        }
        ESP_LOGI(TAG_MQTT, "Waiting for WIFI connection !!");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
