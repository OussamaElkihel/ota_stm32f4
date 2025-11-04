/* UART asynchronous example, that uses separate RX and TX tasks

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "esp_spiffs.h"

#define ACK 0x79
#define NACK 0x1F
#define NUM_BYTES 4     // number of bytes to write

#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)

#define UART_PORT UART_NUM_2

int length = 0; // Length of data to be received
uint8_t checksum = 0; //checksum

// Tasks Handles
TaskHandle_t init_rx_task_handle = NULL;
TaskHandle_t init_tx_task_handle = NULL;

//handle of trigger task
TaskHandle_t trigger_tasks_handle = NULL;

// new task handles
TaskHandle_t status_rx_task_handle = NULL;
TaskHandle_t idle_after_flash_handle = NULL;
// write mem task
TaskHandle_t write_mem_task_handle = NULL;

uint32_t curr_flash_add = 0x08000000;
uint32_t base_flash_add = 0x08000000;

const char *TAG_SPIFFS = "SPIFFS";

char *hex_file_directory = "/spiffs/flash_test.txt";

void init(void)
{
    ////init uart comm
    const uart_config_t uart_config = {
        .baud_rate = 56000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_PORT, 1024 * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ////init SPIFFS
    ESP_LOGI(TAG_SPIFFS, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG_SPIFFS, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG_SPIFFS, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG_SPIFFS, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    ////get SPIFFS partition information
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_SPIFFS, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG_SPIFFS, "Partition size: total: %d, used: %d", total, used);
    }
}

size_t get_hex_size(char *hex_file_directory)
{
    FILE *hex_file = fopen(hex_file_directory, "r");
    size_t size_out = 0;
    char line[32];
    memset(line, 0, sizeof(line));
    if (fgets(line, sizeof(line), hex_file) != NULL)
    {
        size_out = atoi(line);
    }
    fclose(hex_file);
    return size_out;
}

uint8_t *get_data_bytes(uint32_t flash_data) {
    static uint8_t data_bytes[4];  // static to ensure the array persists after the function ends

    data_bytes[0] = (flash_data >> 24) & 0xFF;  // MSB
    data_bytes[1] = (flash_data >> 16) & 0xFF;
    data_bytes[2] = (flash_data >> 8) & 0xFF;
    data_bytes[3] = flash_data & 0xFF;  // LSB

    return data_bytes;
}

uint8_t *get_add_bytes(uint32_t flash_add) {
    static uint8_t add_bytes[4];  // static to ensure the array persists after the function ends

    add_bytes[0] = (flash_add >> 24) & 0xFF;  // MSB
    add_bytes[1] = (flash_add >> 16) & 0xFF;
    add_bytes[2] = (flash_add >> 8) & 0xFF;
    add_bytes[3] = flash_add & 0xFF;  // LSB

    return add_bytes;
}

uint8_t calculate_checksum(uint8_t *data, uint8_t size)
{
    uint8_t result;
    if (size == 0){
        return 0;
    }
    else if (size == 1){
        result = ~data[0];
    }
    else{
        result = data[0];
        for (uint8_t i = 1; i < size; i++)
        {
            result ^= data[i];
        }
    }
    return result;
}

int sendData(const char *logName, const char *data, uint8_t len)
{
    const int txBytes = uart_write_bytes(UART_PORT, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void trigger_tasks(void *arg){
    vTaskDelay(pdMS_TO_TICKS(100));
    xTaskNotifyGive(init_tx_task_handle);
    vTaskDelete(NULL);
}

static void init_tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "INIT_TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while(1){
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    uint8_t data = 0x7F;                                      // Define the data to be sent (0x7F)
    vTaskDelay(100 / portTICK_PERIOD_MS);                     // Wait for bootloader setup (82ms)
    sendData(TX_TASK_TAG, (const char *)&data, sizeof(data)); // Pass a pointer to the data
    xTaskNotifyGive(init_rx_task_handle);
    }
    vTaskDelete(NULL);
}

static void init_rx_task(void *arg)
{
    while(1){
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        static const char *RX_TASK_TAG = "INIT_RX_TASK";
        esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);

        vTaskDelay(10 / portTICK_PERIOD_MS); // Wait for data send (0.16ms)

        uint8_t data0[128];
        ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, (size_t *)&length));
        ESP_LOGI(RX_TASK_TAG, "Received %d bytes", length);
        length = uart_read_bytes(UART_PORT, data0, length, 100);
        ESP_LOGI(RX_TASK_TAG, "rx_data = 0x%X", data0[0]);
        xTaskNotifyGive(write_mem_task_handle);
    }
    vTaskDelete(NULL);
}

static void status_rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "STATUS_RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);

    while (1){
            ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
            uint8_t data0[32];
            ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, (size_t *)&length));
            ESP_LOGI(RX_TASK_TAG, "Received %d bytes", length);
            length = uart_read_bytes(UART_PORT, data0, length, 100);
            for (size_t i = 0; i < 3; i++) {
                ESP_LOGI(RX_TASK_TAG, "0x%02X", data0[i]);
                }
                if(data0[0] != ACK){                           //
                    xTaskNotifyGive(idle_after_flash_handle);   //is this good ?
                    vTaskDelete(NULL);                          //
                }
            xTaskNotifyGive(write_mem_task_handle);
    }
}

static void write_mem(void *arg)
{
    int num_cycle = 0;
    FILE *hex_file = fopen(hex_file_directory, "r");
    printf("size in file = %d\n", get_hex_size(hex_file_directory));
    static const char *WMTAG = "WM_TASK";
    esp_log_level_set(WMTAG, ESP_LOG_INFO);

    static const char *COMMAND_TX_TAG = "COMMAND_TX_TASK"; 
    esp_log_level_set(COMMAND_TX_TAG, ESP_LOG_INFO);

    static const char *ADR_TX_TAG = "ADR_TX_TASK";
    esp_log_level_set(ADR_TX_TAG, ESP_LOG_INFO);

    static const char *NUMBYTES_TX_TAG = "NUMBYTES_TX_TASK"; 
    esp_log_level_set(NUMBYTES_TX_TAG, ESP_LOG_INFO);

    while (curr_flash_add != base_flash_add + get_hex_size(hex_file_directory))
    {
        uint8_t *add_bytes = get_add_bytes(curr_flash_add);     //get bytes from uint32_t for address
        char line[64];
        memset(line, 0, sizeof(line));
        uint32_t flash_data = 0;
        if(num_cycle == 0){                     //skip first line
            fgets(line, sizeof(line), hex_file);
        }
        if(fgets(line, sizeof(line), hex_file) != NULL){
            flash_data = strtoul(line, NULL, 16);
        }
        uint8_t *data_bytes = get_data_bytes(flash_data);       //get bytes from uint32_t for data

        ////////// send write command
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

            uint8_t command0 = 0x31;
            uint8_t command1 = 0xCE;
            sendData(COMMAND_TX_TAG, (const char *)&command0, sizeof(command0)); // send command data0
            sendData(COMMAND_TX_TAG, (const char *)&command1, sizeof(command1)); // send command data1
            xTaskNotifyGive(status_rx_task_handle);
        //////////
        // wait for ack
        ////////// send start address
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

            uint8_t addr[4] = {add_bytes[0], add_bytes[1], add_bytes[2], add_bytes[3]}; // start address

            uint8_t checksum = calculate_checksum(addr, 4); // calculate checksum

            sendData(ADR_TX_TAG, (const char *)&addr[0], sizeof(addr[0])); // send command data0
            sendData(ADR_TX_TAG, (const char *)&addr[1], sizeof(addr[1])); // send command data1
            sendData(ADR_TX_TAG, (const char *)&addr[2], sizeof(addr[2])); // send command data2
            sendData(ADR_TX_TAG, (const char *)&addr[3], sizeof(addr[3])); // send command data3
            ESP_LOGI(ADR_TX_TAG, "Sent addr data");

            sendData(ADR_TX_TAG, (const char *)&checksum, sizeof(checksum)); // send checksum
            ESP_LOGI(ADR_TX_TAG, "Sent checksum");
            xTaskNotifyGive(status_rx_task_handle);
        //////////
        // wait for ack
        ////////// send number of bytes to write
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        //if (notifications > 0)
        //{
            uint8_t data0 = NUM_BYTES - 1;

            sendData(NUMBYTES_TX_TAG, (const char *)&data0, sizeof(data0)); // send num of bytes
            ESP_LOGI(NUMBYTES_TX_TAG, "sent number of bytes");

            ////////// send bytes to write
            uint8_t bytestowrite[5] = {data_bytes[0], data_bytes[1], data_bytes[2], data_bytes[3], data0};
            printf("data in send : %d  %d  %d  %d\n", data_bytes[0], data_bytes[1], data_bytes[2], data_bytes[3]);

            checksum = calculate_checksum(bytestowrite, 5); // calculate checksum

            sendData(NUMBYTES_TX_TAG, (const char *)&bytestowrite[0], sizeof(bytestowrite[0])); // send command data0
            sendData(NUMBYTES_TX_TAG, (const char *)&bytestowrite[1], sizeof(bytestowrite[1])); // send command data1
            sendData(NUMBYTES_TX_TAG, (const char *)&bytestowrite[2], sizeof(bytestowrite[2])); // send command data1
            sendData(NUMBYTES_TX_TAG, (const char *)&bytestowrite[3], sizeof(bytestowrite[3])); // send command data1
            ESP_LOGI(NUMBYTES_TX_TAG, "Sent data to write");

            sendData(NUMBYTES_TX_TAG, (const char *)&checksum, sizeof(checksum)); // send checksum
            ESP_LOGI(NUMBYTES_TX_TAG, "Sent checksum");
            xTaskNotifyGive(status_rx_task_handle);
            ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        //}
        ESP_LOGI(WMTAG, "END OF CYCLE %d", num_cycle);
        xTaskNotifyGive(write_mem_task_handle);
        curr_flash_add += 0x04;
        num_cycle += 1;
    }
    xTaskNotifyGive(idle_after_flash_handle);
    fclose(hex_file);
    esp_vfs_spiffs_unregister(NULL);
    vTaskDelete(NULL);
}

static void idle_after_flash(void *arg){
    while(true){
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    static const char *IDLE_TAG = "IDLE_TASK";
    esp_log_level_set(IDLE_TAG, ESP_LOG_INFO);
    ESP_LOGI(IDLE_TAG, "FLASH FINISHED !!\n");
    vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    init();
    xTaskCreate(trigger_tasks, "trigger_tasks_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &trigger_tasks_handle);
    xTaskCreate(init_tx_task, "uart_init_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &init_tx_task_handle);
    xTaskCreate(init_rx_task, "uart_init_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &init_rx_task_handle);
    xTaskCreate(status_rx_task, "uart_status_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &status_rx_task_handle);
    xTaskCreate(write_mem, "uart_write_mem_task", 1024 * 10, NULL, configMAX_PRIORITIES - 1, &write_mem_task_handle);
    xTaskCreate(idle_after_flash, "idle_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &idle_after_flash_handle);
}
