#ifndef WIFICONN_H
#define WIFICONN_H

#include "stdio.h"
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "esp_event.h"       

extern const char *ssid;
extern const char *pass;
extern int retry_num;

extern TaskHandle_t wifi_conn_task_handle;
extern int wifi_connected;

void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void wifi_connection(void *arg);

#endif