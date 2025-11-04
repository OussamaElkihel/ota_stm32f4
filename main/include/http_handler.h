#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include "esp_http_client.h"

//Dropbox access token is defined here 
#define DROPBOX_TOKEN "Bearer sl.CEv0PFSetXMtOI-0bOkJ3JyUCQoMDOi3DIzyy_NnzcZMtz54MHFp5o-6m8Z0YDyH5aMpcdnAFj7edCKfW67pd23di4yvSxLCxq2DyDfdN2NEJHXTBR5OY2l3YT3nUXZnCdaThX7bj3kZ" 
//Dropbox file path is defined here
#define DROPBOX_FILE_PATH "/led_test0.txt"

#define MAX_HTTP_OUTPUT_BUFFER 2048
#define OTA_FILE "/spiffs/led_test0.txt"

extern const char *TAG;

extern FILE *f0;

extern TaskHandle_t download_task_handle;

esp_err_t _http_event_handler(esp_http_client_event_t *evt);
bool download_from_dropbox(void);
//void read_file(void);
void file_download_task(void *pvParameters);

#endif
