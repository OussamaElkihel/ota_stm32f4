#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "esp_spiffs.h"
#include "esp_partition.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "mqtt_main_app.h"
#include "wifi_conn.h"
#include "test_main.h"
#include "http_handler.h"

int length = 0;       // Length of data to be received
uint8_t checksum = 0; // checksum

// init tasks Handles
TaskHandle_t init_rx_task_handle = NULL;
TaskHandle_t init_tx_task_handle = NULL;

// handle of trigger task
TaskHandle_t trigger_tasks_handle = NULL;

// new task handles
TaskHandle_t status_write_rx_task_handle = NULL;
TaskHandle_t status_erase_rx_task_handle = NULL;
TaskHandle_t idle_after_flash_handle = NULL;
TaskHandle_t loading_bar_handle = NULL;

// write, erase mem task
TaskHandle_t write_mem_task_handle = NULL;
TaskHandle_t erase_mem_task_handle = NULL;
TaskHandle_t reset_task_handle = NULL;

uint32_t curr_flash_add = 0x08000000;
uint32_t base_flash_add = 0x08000000;

uint32_t global_progress = 0; // for loading_bar
uint8_t barWidth = 50;        // loading bar width

const char *TAG_SPIFFS = "SPIFFS";

FILE *f0;

//init uart and SPIFFS
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

    uart_driver_install(UART_PORT, 1024 * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    ////init SPIFFS
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 10,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG_SPIFFS, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG_SPIFFS, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG_SPIFFS, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    ESP_LOGI(TAG_SPIFFS, "Performing SPIFFS_check().");
    ret = esp_spiffs_check(conf.partition_label);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_SPIFFS, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
        return;
    } else {
        ESP_LOGI(TAG_SPIFFS, "SPIFFS_check() successful");
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_SPIFFS, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    } else {
        ESP_LOGI(TAG_SPIFFS, "Partition size: total: %d, used: %d", total, used);
    }

    // Check consistency of reported partition size info.
    if (used > total) {
        ESP_LOGW(TAG_SPIFFS, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
        ret = esp_spiffs_check(conf.partition_label);
        // Could be also used to mend broken files, to clean unreferenced pages, etc.
        // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
        if (ret != ESP_OK) {
            ESP_LOGE(TAG_SPIFFS, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return;
        } else {
            ESP_LOGI(TAG_SPIFFS, "SPIFFS_check() successful");
        }
    }

    // create file
    FILE* f0 = fopen("/spiffs/led_test0.txt", "w");
    printf("pointer value after creation: %p\n", f0);
    if (f0 == NULL) {
        ESP_LOGE(TAG_SPIFFS, "Failed to open file for writing");
        return;
    }
    fclose(f0);
}

void init_gpio(void)
{
    gpio_config_t conf_gpio = {
        .pin_bit_mask = 1ULL << GPIO_NUM_25 | 1ULL << GPIO_NUM_26, // GPIO_NUM_25 --> NRST
        .mode = GPIO_MODE_OUTPUT | GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE};

    esp_err_t err;
    err = gpio_config(&conf_gpio);
    if (err != ESP_OK)
    {
        printf("conf error\n");
        return;
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

uint8_t *get_msb_lsb(uint16_t tot_val)
{
    static uint8_t msb_lsb[2];
    uint8_t lsb_val = tot_val & 0xFF;
    uint8_t msb_val = (tot_val >> 8) & 0xFF;
    msb_lsb[0] = msb_val;
    msb_lsb[1] = lsb_val;
    return msb_lsb;
}

uint8_t *get_data_bytes(uint32_t flash_data)
{
    static uint8_t data_bytes[4]; // static to ensure the array persists after the function ends

    data_bytes[0] = (flash_data >> 24) & 0xFF; // MSB
    data_bytes[1] = (flash_data >> 16) & 0xFF;
    data_bytes[2] = (flash_data >> 8) & 0xFF;
    data_bytes[3] = flash_data & 0xFF; // LSB

    return data_bytes;
}

uint8_t *get_add_bytes(uint32_t flash_add)
{
    static uint8_t add_bytes[4]; // static to ensure the array persists after the function ends

    add_bytes[0] = (flash_add >> 24) & 0xFF; // MSB
    add_bytes[1] = (flash_add >> 16) & 0xFF;
    add_bytes[2] = (flash_add >> 8) & 0xFF;
    add_bytes[3] = flash_add & 0xFF; // LSB

    return add_bytes;
}

uint8_t calculate_checksum(uint8_t *data, uint8_t size)
{
    uint8_t result;
    if (size == 0)
    {
        return 0;
    }
    else if (size == 1)
    {
        result = ~data[0];
    }
    else
    {
        result = data[0];
        for (uint8_t i = 1; i < size; i++)
        {
            result ^= data[i];
        }
    }
    return result;
}

//helper function to send data using UART
int sendData(const char *logName, const char *data, uint8_t len)
{
    const int txBytes = uart_write_bytes(UART_PORT, data, len);
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));
    return txBytes;
}

//tasks that triggers init_tx_task()
//this task first waits for start command in PyQt app
void trigger_tasks(void *arg)
{
    static const char *TRIG_TASK_TAG = "TRIGGER_TASK";
    esp_log_level_set(TRIG_TASK_TAG, ESP_LOG_INFO);
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    while (1){
        vTaskDelay(pdMS_TO_TICKS(100));
        if (start != 0)
        {
            ESP_LOGI(TRIG_TASK_TAG, "Received start command");
            uart_flush(UART_PORT);
            xTaskNotifyGive(init_tx_task_handle);
            vTaskDelete(NULL);
        }
        else{
            ESP_LOGI(TRIG_TASK_TAG, "Did not receive start command");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//task that sends 0x7F to initialize UART bootloader
void init_tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "INIT_TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);

    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    uint8_t data = 0x7F;                                      // Define the data to be sent (0x7F)
    vTaskDelay(pdMS_TO_TICKS(100));                     // Wait for bootloader setup (82ms)
    sendData(TX_TASK_TAG, (const char *)&data, sizeof(data)); // Pass a pointer to the data
    xTaskNotifyGive(init_rx_task_handle);

    vTaskDelete(NULL);  //delete task after init
}

//task to receive status of bootloader initialization
//ACK=0x79, NACK=0x1F
void init_rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "INIT_RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);

    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(10));  // Wait for data send (0.16ms)

    uint8_t data0[128];
    ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, (size_t *)&length));
    ESP_LOGI(RX_TASK_TAG, "Received %d bytes", length);
    length = uart_read_bytes(UART_PORT, data0, length, 100);
    ESP_LOGI(RX_TASK_TAG, "rx_data = 0x%X", data0[0]);
    xTaskNotifyGive(erase_mem_task_handle); // give exec to erase_mem

    vTaskDelete(NULL);
}

//this task is used by write_mem task to receive write status
void status_write_rx_task(void *arg)
{
    static const char *RX_WR_TASK_TAG = "STATUS_WRITE_RX_TASK";
    esp_log_level_set(RX_WR_TASK_TAG, ESP_LOG_INFO);

    while (1)
    {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        uint8_t data0[32];
        ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, (size_t *)&length));
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));
        length = uart_read_bytes(UART_PORT, data0, length, 100);
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));
        if (data0[0] != ACK)
        {
            ESP_LOGI(RX_WR_TASK_TAG, "Error occured while flashing !!\n Starting Reset !!");
            vTaskDelay(pdMS_TO_TICKS(500));
            xTaskNotifyGive(reset_task_handle);
            ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        }
        xTaskNotifyGive(write_mem_task_handle);
    }
}

//this task is used by erase_mem task to receive erase status
void status_erase_rx_task(void *arg)
{
    static const char *RX_ERASE_TASK_TAG = "STATUS_ERASE_RX_TASK";
    esp_log_level_set(RX_ERASE_TASK_TAG, ESP_LOG_INFO);

    while (1)
    {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        uint8_t data0[32];
        ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_PORT, (size_t *)&length));
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));
        length = uart_read_bytes(UART_PORT, data0, length, 100);
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));
        if (data0[0] != ACK)
        {
            ESP_LOGI(RX_ERASE_TASK_TAG, "Error occured while erasing !!\n Starting Reset !!");
            vTaskDelay(pdMS_TO_TICKS(500));
            xTaskNotifyGive(reset_task_handle);
            ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        }
        xTaskNotifyGive(erase_mem_task_handle);
    }
}

void write_mem(void *arg)
{
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    xTaskNotifyGive(write_mem_task_handle);
    int num_cycle = 0;          // track number of writes
    FILE *hex_file = fopen(OTA_FILE, "r");
    static const char *WMTAG = "WM_TASK";
    esp_log_level_set(WMTAG, ESP_LOG_INFO);

    static const char *COMMAND_TX_TAG = "COMMAND_TX_TASK";
    esp_log_level_set(COMMAND_TX_TAG, ESP_LOG_INFO);

    static const char *ADR_TX_TAG = "ADR_TX_TASK";
    esp_log_level_set(ADR_TX_TAG, ESP_LOG_INFO);

    static const char *NUMBYTES_TX_TAG = "NUMBYTES_TX_TASK";
    esp_log_level_set(NUMBYTES_TX_TAG, ESP_LOG_INFO);

    while (curr_flash_add != base_flash_add + get_hex_size(OTA_FILE))
    {
        uint8_t *add_bytes = get_add_bytes(curr_flash_add); // get bytes from uint32_t for address
        char line[64];
        memset(line, 0, sizeof(line));
        uint32_t flash_data = 0;
        if (num_cycle == 0)
        { // skip first line
            fgets(line, sizeof(line), hex_file);
        }
        if (fgets(line, sizeof(line), hex_file) != NULL)
        {
            flash_data = strtoul(line, NULL, 16);
        }
        uint8_t *data_bytes = get_data_bytes(flash_data); // get bytes from uint32_t for data

        ////////// send write command
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));

        uint8_t command0 = 0x31;
        uint8_t command1 = 0xCE;
        sendData(COMMAND_TX_TAG, (const char *)&command0, sizeof(command0)); // send command data0
        sendData(COMMAND_TX_TAG, (const char *)&command1, sizeof(command1)); // send command data1
        xTaskNotifyGive(status_write_rx_task_handle);
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
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));

        sendData(ADR_TX_TAG, (const char *)&checksum, sizeof(checksum)); // send checksum
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));
        xTaskNotifyGive(status_write_rx_task_handle);
        //////////
        // wait for ack
        ////////// send number of bytes to write
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        uint8_t data0 = NUM_BYTES - 1;

        sendData(NUMBYTES_TX_TAG, (const char *)&data0, sizeof(data0)); // send num of bytes
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));

        ////////// send bytes to write
        uint8_t bytestowrite[5] = {data_bytes[0], data_bytes[1], data_bytes[2], data_bytes[3], data0};

        checksum = calculate_checksum(bytestowrite, 5); // calculate checksum

        sendData(NUMBYTES_TX_TAG, (const char *)&bytestowrite[0], sizeof(bytestowrite[0])); // send command data0
        sendData(NUMBYTES_TX_TAG, (const char *)&bytestowrite[1], sizeof(bytestowrite[1])); // send command data1
        sendData(NUMBYTES_TX_TAG, (const char *)&bytestowrite[2], sizeof(bytestowrite[2])); // send command data1
        sendData(NUMBYTES_TX_TAG, (const char *)&bytestowrite[3], sizeof(bytestowrite[3])); // send command data1
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));

        sendData(NUMBYTES_TX_TAG, (const char *)&checksum, sizeof(checksum)); // send checksum
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));
        xTaskNotifyGive(status_write_rx_task_handle);
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));
        xTaskNotifyGive(write_mem_task_handle);
        curr_flash_add += 0x04;
        num_cycle += 1;
        global_progress = 4 * num_cycle;
        if (num_cycle == 1)
        {                                        // notify loading_bar to start only when num_cycle = 1
            xTaskNotifyGive(loading_bar_handle); // in order to not divide by zero
        }
    }
    xTaskNotifyGive(idle_after_flash_handle);
    fclose(hex_file);
    esp_vfs_spiffs_unregister(NULL);
    vTaskDelete(NULL);
}

// erase 2 sectors only (sector 0 and 1)
void erase_mem(void *arg)
{
    static const char *EMTAG = "EM_TASK"; // erase memory task tag
    esp_log_level_set(EMTAG, ESP_LOG_INFO);

    static const char *COMMAND_TX_TAG = "COMMAND_TX_TASK";
    esp_log_level_set(COMMAND_TX_TAG, ESP_LOG_INFO);

    static const char *NUM_PAGE_TX_TAG = "NUM_PAGE_TX_TASK";
    esp_log_level_set(NUM_PAGE_TX_TAG, ESP_LOG_INFO);

    ////////// send erase command
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    uint8_t command0 = 0x44;
    uint8_t command1 = 0xBB;
    sendData(COMMAND_TX_TAG, (const char *)&command0, sizeof(command0)); // send command data0
    sendData(COMMAND_TX_TAG, (const char *)&command1, sizeof(command1)); // send command data1
    xTaskNotifyGive(status_erase_rx_task_handle);

    //////////
    // wait for ack
    ////////// send num of pages to erase
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    uint16_t num_pages = NUM_PAGES; // N+1 pages are erased
    uint8_t *num_pages_tot = get_msb_lsb(num_pages);
    uint8_t num_pagesMSB = num_pages_tot[0];
    uint8_t num_pagesLSB = num_pages_tot[1];
    sendData(NUM_PAGE_TX_TAG, (const char *)&num_pagesMSB, sizeof(num_pagesMSB)); // send number of pages
    sendData(NUM_PAGE_TX_TAG, (const char *)&num_pagesLSB, sizeof(num_pagesLSB));
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST)); // delay

    ///////// send page codes
    uint16_t page0 = 0;
    uint16_t page1 = 1; // page codes
    uint8_t *page0_tot = get_msb_lsb(page0);
    uint8_t page0MSB = page0_tot[0];
    uint8_t page0LSB = page0_tot[1];
    uint8_t *page1_tot = get_msb_lsb(page1);
    uint8_t page1MSB = page1_tot[0];
    uint8_t page1LSB = page1_tot[1];
    sendData(NUM_PAGE_TX_TAG, (const char *)&page0MSB, sizeof(page0MSB)); // send data0
    sendData(NUM_PAGE_TX_TAG, (const char *)&page0LSB, sizeof(page0LSB));
    sendData(NUM_PAGE_TX_TAG, (const char *)&page1MSB, sizeof(page1MSB)); // send data1
    sendData(NUM_PAGE_TX_TAG, (const char *)&page1LSB, sizeof(page1LSB));
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST)); // delay

    //////// calculate and send checksum
    uint8_t total_val[6] = {num_pagesMSB, num_pagesLSB, page0MSB, page0LSB, page1MSB, page1LSB};
    uint8_t checksum = calculate_checksum(total_val, 6);                  // calculate checksum
    sendData(NUM_PAGE_TX_TAG, (const char *)&checksum, sizeof(checksum)); // send data
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_CONST));

    xTaskNotifyGive(status_erase_rx_task_handle);
    //////////
    // wait for ack
    //////////
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(DELAY_BET_COMM));
    xTaskNotifyGive(write_mem_task_handle);
    ESP_LOGI(EMTAG, "Erased Flash Memory!!");
    vTaskDelete(NULL);
}

// call this after flash is finished or flash failed
void idle_after_flash(void *arg)
{
    while (true)
    {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        static const char *IDLE_TAG = "IDLE_TASK";
        esp_log_level_set(IDLE_TAG, ESP_LOG_INFO);
        ESP_LOGI(IDLE_TAG, "FLASH FINISHED !!\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

//task to display loading bar
void loading_bar(void *arg)
{
    static const char *LOADING_TAG = "LOADING_TASK";
    esp_log_level_set(LOADING_TAG, ESP_LOG_INFO);
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    ESP_LOGI(LOADING_TAG, "Flashing...");
    uint32_t target_val = get_hex_size(OTA_FILE);
    while (global_progress < target_val)
    {
        printf("\r[");
        uint16_t pos = global_progress * barWidth / target_val;
        for (int i = 0; i < barWidth; i++)
        {
            if (i < pos)
            {
                printf("#");
            }
            else
            {
                printf(" ");
            }
        }
        printf("] %d%%", (int)(100 * global_progress / target_val));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    vTaskDelete(NULL);
}

//helper function used by reset_task
void restart_tasks(void)
{
    //delete tasks first
    vTaskDelete(status_write_rx_task_handle);
    vTaskDelete(status_erase_rx_task_handle);
    vTaskDelete(write_mem_task_handle);
    vTaskDelete(erase_mem_task_handle);
    vTaskDelete(idle_after_flash_handle);
    vTaskDelete(loading_bar_handle);
    //re create tasks
    xTaskCreate(trigger_tasks, "trigger_tasks_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &trigger_tasks_handle);
    xTaskCreate(init_tx_task, "uart_init_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &init_tx_task_handle);
    xTaskCreate(init_rx_task, "uart_init_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &init_rx_task_handle);
    xTaskCreate(status_write_rx_task, "uart_status_write_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &status_write_rx_task_handle);
    xTaskCreate(status_erase_rx_task, "uart_status_erase_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &status_erase_rx_task_handle);
    xTaskCreate(write_mem, "uart_write_mem_task", 1024 * 10, NULL, configMAX_PRIORITIES - 1, &write_mem_task_handle);
    xTaskCreate(erase_mem, "uart_erase_mem_task", 1024 * 5, NULL, configMAX_PRIORITIES - 1, &erase_mem_task_handle);
    xTaskCreate(idle_after_flash, "idle_task", 1024 * 2, NULL, configMAX_PRIORITIES - 3, &idle_after_flash_handle);
    xTaskCreate(loading_bar, "loading_bar_task", 1024 * 5, NULL, configMAX_PRIORITIES - 3, &loading_bar_handle);
}

// reset task
void reset_task(void *arg)
{
    static const char *RST_TAG = "RESET_TASK";
    esp_log_level_set(RST_TAG, ESP_LOG_INFO);
    while (1)
    {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        ESP_LOGI(RST_TAG, "Performing STM32F4 Reset !!"); // reset stm32f4 first
        esp_err_t err;
        err = gpio_set_level(GPIO_NUM_25, 0);
        if (err != ESP_OK)
        {
            printf("invalid params\n");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        err = gpio_set_level(GPIO_NUM_25, 1);
        if (err != ESP_OK)
        {
            printf("invalid params\n");
            return;
        }

        ESP_LOGI(RST_TAG, "Performing ESP32 Reset !!"); // reset esp32 second
        vTaskDelay(pdMS_TO_TICKS(2000));
        restart_tasks();
        vTaskDelay(pdMS_TO_TICKS(1000));
        xTaskNotifyGive(trigger_tasks_handle);      //start trigger task
    }
}

void app_main(void)
{
    init();      // init uart and spiffs
    init_gpio(); // init gpio

    //wifi related
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //wifi task
    xTaskCreate(wifi_connection, "wifi_conn_task", 1024 * 5, NULL, configMAX_PRIORITIES - 1, &wifi_conn_task_handle);

    //mqtt task
    xTaskCreate(mqtt_app_start, "mqtt_app_task", 1024 * 5, NULL, configMAX_PRIORITIES - 1, &mqtt_start_task_handle);

    //download file task
    xTaskCreate(file_download_task, "file_download_task", 1024 * 5, NULL, configMAX_PRIORITIES - 1, &download_task_handle);

    // flash related
    xTaskCreate(trigger_tasks, "trigger_tasks_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &trigger_tasks_handle);
    xTaskCreate(init_tx_task, "uart_init_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &init_tx_task_handle);
    xTaskCreate(init_rx_task, "uart_init_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &init_rx_task_handle);
    xTaskCreate(status_write_rx_task, "uart_status_write_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &status_write_rx_task_handle);
    xTaskCreate(status_erase_rx_task, "uart_status_erase_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, &status_erase_rx_task_handle);
    xTaskCreate(write_mem, "uart_write_mem_task", 1024 * 10, NULL, configMAX_PRIORITIES - 1, &write_mem_task_handle);
    xTaskCreate(erase_mem, "uart_erase_mem_task", 1024 * 5, NULL, configMAX_PRIORITIES - 1, &erase_mem_task_handle);
    xTaskCreate(reset_task, "uart_reset_task", 1024 * 5, NULL, configMAX_PRIORITIES - 1, &reset_task_handle);
    xTaskCreate(idle_after_flash, "idle_task", 1024 * 2, NULL, configMAX_PRIORITIES - 3, &idle_after_flash_handle);
    xTaskCreate(loading_bar, "loading_bar_task", 1024 * 5, NULL, configMAX_PRIORITIES - 3, &loading_bar_handle);
}
