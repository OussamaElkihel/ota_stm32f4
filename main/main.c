#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "soc/spi_pins.h"
#include "driver/spi_master.h"



/*UART Pins definition */
#define UART_PORT UART_NUM_0
#define UART_TX_PIN GPIO_NUM_1
#define UART_RX_PIN GPIO_NUM_3
#define UART_RX_BUFF_Z (1024 * 2)
#define UART_TX_BUFF_Z (1024 * 2)
#define BOOT0_PIN GPIO_NUM_15
#define BOOT1_PIN GPIO_NUM_14
/*Flash Memory*/
#define HOST_ID SPI2_HOST 
#define MOSI    SPI2_IOMUX_PIN_NUM_MOSI
#define MISO    SPI2_IOMUX_PIN_NUM_MISO
#define CLK     SPI2_IOMUX_PIN_NUM_CLK
#define CS      SPI2_IOMUX_PIN_NUM_CS
#define HD      SPI2_IOMUX_PIN_NUM_HD
#define WP      SPI2_IOMUX_PIN_NUM_WP


esp_err_t uart_init();
void flash_init();
void app_main(void)
{
    esp_err_t err ;
    uart_init();
    char* error_1 = "error";
    char* error_2 = "failed to write";
    char* error_3 = "failed to read";
    char* test_str = "Hello Lkihel\n";
    char* read_buff = malloc(14 * sizeof(char));
    //uart_write_bytes(UART_PORT, (const char*)test_str, strlen(test_str));
    spi_bus_config_t bus_config = {
        .mosi_io_num = MOSI ,
        .miso_io_num = MISO ,
        .sclk_io_num = CLK ,
        .quadhd_io_num = HD ,
        .quadwp_io_num = WP
    };
    spi_bus_initialize(HOST_ID , &bus_config ,SPI_DMA_DISABLED);
    // spi_bus_add_flash_device()
    err = esp_flash_init(NULL);
    if (err != ESP_OK){
        uart_write_bytes(UART_PORT, error_1, sizeof(error_1));
    }
    err = esp_flash_write(NULL ,test_str, (uint32_t)0x3F400000 , sizeof(test_str));
    if (err != ESP_OK){
        uart_write_bytes(UART_PORT, error_2, sizeof(error_2));
    }
    err = esp_flash_read(NULL ,read_buff , (uint32_t)0x3F400000 , sizeof(read_buff));
    if (err != ESP_OK){
        uart_write_bytes(UART_PORT, error_3, sizeof(error_3));
    }
    while(1){
    uart_write_bytes(UART_PORT, read_buff, strlen(read_buff));
    esp_rom_delay_us(1000000);
    }

}

esp_err_t  uart_init(){
    esp_err_t err ;
      const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    err = uart_param_config(UART_PORT, &uart_config);
    if (err != ESP_OK){
        return err;
    }
    err = uart_set_pin(UART_PORT , UART_TX_PIN ,UART_RX_PIN , UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
    if (err != ESP_OK){
        return err;
    } 
    uart_driver_install(UART_PORT ,UART_RX_BUFF_Z , UART_TX_BUFF_Z , 10 , NULL , 0);
    return ESP_OK;
}
