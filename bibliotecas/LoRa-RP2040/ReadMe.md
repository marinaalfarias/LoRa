# LoRa-RP2040
Esses arquivos têm como objetivo comunicar com um módulo LoRa via SPI. 
## LoRa-RP2040.c
Esse arquivo foi feito especificamente para o RP2040 e utiliza as bibliotecas Pico SDK (como ``hardware/spi.h``), utiliza, também, o acesso de baixo nível ao SPI e GPIO.

## LoRa-RP2040.h
Esse arquivo define a interface de uso do driver LoRa específico para o RP2040. Ele funciona como um **driver de alto nível** para aplicações baseadas no RP2040. 
