# OTA project

This is an academic project aimed at implementing software update solution for the stm32f429 series MCU.
The MCU used to update stm32 software is esp32.
The esp32 fetches update file from dropbox, downloads it to flash (spiffs) and sends the update through UART.
