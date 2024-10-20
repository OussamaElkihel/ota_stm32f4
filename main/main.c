#include <stdio.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_rom_sys.h"

/*Pin definitions */
#define UART_PORT UART_NUM_0
#define UART_TX_PIN GPIO_NUM_1
#define UART_RX_PIN GPIO_NUM_3
#define UART_RX_BUFF_Z (1024 * 2)
#define UART_TX_BUFF_Z (1024 * 2)
#define BOOT0_PIN GPIO_NUM_15
#define BOOT1_PIN GPIO_NUM_14

esp_err_t uart_init();
void app_main(void)
{
    uart_init();
    char* test_str = "Hello Lkihel\n";
    //uart_write_bytes(UART_PORT, (const char*)test_str, strlen(test_str));
    while(1){
    uart_write_bytes(UART_PORT, (const char*)test_str, strlen(test_str));
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
}