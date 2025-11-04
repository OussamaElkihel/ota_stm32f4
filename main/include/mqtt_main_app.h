#ifndef MQTTMAINAPP_H
#define MQTTMAINAPP_H

#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "esp_event.h"

extern uint8_t start;       //var to start flashing: =0 --> do not start, !=0 --> start
extern TaskHandle_t mqtt_start_task_handle;

void log_error_if_nonzero(const char *message, int error_code);

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

void mqtt_app_start(void *arg);

#endif