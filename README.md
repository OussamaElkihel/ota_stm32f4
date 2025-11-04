# OTA project

This is an academic project aimed at implementing software update solution for the stm32f429 series MCU.
The MCU used to update stm32 software is esp32.
The esp32 fetches update file from dropbox, downloads it to flash (spiffs) and sends the update through UART.

If you want to try this code:
-create esp32 project using empty template
-copy the src files in main folder to your main
-add this line to your Cmakelists (before project name):
  ```sh
  set(COMPONENTS main esp_wifi esp_http_client nvs_flash esp_netif spiffs mqtt)
  ```
-build the project using idf.py:
  ```sh
  idf.py build
  ```
-flash the esp32:
  ```sh
  idf.py flash
  ```