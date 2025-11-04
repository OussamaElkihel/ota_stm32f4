#include <stdio.h>     
#include <sys/param.h>        
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "esp_log.h"                
#include "esp_tls.h"         
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "http_handler.h"
#include "test_main.h"
#include "mqtt_main_app.h"

const char *TAG = "DropboxDownload";

TaskHandle_t download_task_handle = NULL;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);

            // Clean the buffer in case of a new request
            if (output_len == 0 && evt->user_data)
            {
                // We are just starting to copy the output data into the user data buffer
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }

            // Check if the response is not chunked
            if (!esp_http_client_is_chunked_response(evt->client))
            {
                // If the user data buffer is available, copy data into it
                int copy_len = 0;
                if (evt->user_data)
                {
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len)
                    {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                }
                else
                {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL)
                    {
                        output_buffer = (char *)calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL)
                        {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len)
                    {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                    // open file in spiffs and write into it
                    f0 = fopen(OTA_FILE, "a");
                    printf("pointer value in http handler: %p\n", f0);
                    if (f0 == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to open file for writing");
                        return ESP_FAIL;
                    }
                    char event_data[evt->data_len];
                    memcpy(event_data, (char *)evt->data, evt->data_len);
                    event_data[evt->data_len] = '\0'; // null terminator
                    fprintf(f0, "%s", event_data);
                    fflush(f0);
                    fclose(f0);
                }
                output_len += copy_len;
            }
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
#if CONFIG_EXAMPLE_ENABLE_RESPONSE_BUFFER_DUMP
                ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
#endif
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

bool download_from_dropbox(void) {

    esp_http_client_config_t config = {
        .url = "https://content.dropboxapi.com/2/files/download",
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set the HTTP method to POST
    esp_http_client_set_method(client, HTTP_METHOD_POST);

    // Set the "Authorization" header
    esp_http_client_set_header(client, "Authorization", DROPBOX_TOKEN);

    // Set the "Dropbox-API-Arg" header
    char dropbox_api_arg[128];
    snprintf(dropbox_api_arg, sizeof(dropbox_api_arg), "{\"path\":\"%s\"}", DROPBOX_FILE_PATH);
    //const char *dropbox_api_arg = "{\"path\":\"/led_test0_claude.txt\"}";
    esp_http_client_set_header(client, "Dropbox-API-Arg", dropbox_api_arg);

    // Perform the HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRId64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));

    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        return false;
    }

    // Cleanup
    esp_http_client_cleanup(client);
    return true;
}

void file_download_task(void *pvParameters) {
    while (1) {
        ESP_LOGI(TAG, "In task: file_download_task()");
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Starting file download...");
        vTaskDelay(pdMS_TO_TICKS(100));
        if (download_from_dropbox()) {
            ESP_LOGI(TAG, "File download completed successfully");
            vTaskDelay(pdMS_TO_TICKS(100));
            xTaskNotifyGive(mqtt_start_task_handle);
        } else {
            ESP_LOGE(TAG, "File download failed, Restarting...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        }
    }
}