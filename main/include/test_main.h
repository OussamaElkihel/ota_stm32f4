#ifndef TESTMAIN_H
#define TESTMAIN_H

#include <stdio.h>
#include <stdlib.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ACK 0x79
#define NACK 0x1F
#define NUM_BYTES 4 // number of bytes to write

#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)

#define UART_PORT UART_NUM_2

#define TASK_DELAY_CONST 10
#define DELAY_BET_COMM 600 // min tested delay between commands write and erase

#define NUM_PAGES 1 // number of pages to erase

extern int length;
extern uint8_t checksum;

extern uint32_t curr_flash_add;
extern uint32_t base_flash_add;

// init tasks Handles
extern TaskHandle_t init_rx_task_handle;
extern TaskHandle_t init_tx_task_handle;

// handle of trigger task
extern TaskHandle_t trigger_tasks_handle;

// new task handles
extern TaskHandle_t status_write_rx_task_handle;
extern TaskHandle_t status_erase_rx_task_handle;
extern TaskHandle_t idle_after_flash_handle;
extern TaskHandle_t loading_bar_handle;

// write, erase mem task
extern TaskHandle_t write_mem_task_handle;
extern TaskHandle_t erase_mem_task_handle;
extern TaskHandle_t reset_task_handle;

void init(void);

void init_gpio(void);

size_t get_hex_size(char *hex_file_directory);

uint8_t *get_msb_lsb(uint16_t tot_val);

uint8_t *get_data_bytes(uint32_t flash_data);

uint8_t *get_add_bytes(uint32_t flash_add);

uint8_t calculate_checksum(uint8_t *data, uint8_t size);

int sendData(const char *logName, const char *data, uint8_t len);

void trigger_tasks(void *arg);

void init_tx_task(void *arg);

void init_rx_task(void *arg);

void status_write_rx_task(void *arg);

void status_erase_rx_task(void *arg);

void write_mem(void *arg);

void erase_mem(void *arg);

void idle_after_flash(void *arg);

void loading_bar(void *arg);

void restart_tasks(void);

void reset_task(void *arg);

#endif