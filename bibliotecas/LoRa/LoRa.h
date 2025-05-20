#ifndef LORA_H
#define LORA_H

#include <stdint.h>

void LoRa_init();
void LoRa_setFrequency(long frequency);
void LoRa_setTxPower(int level, int outputPin);
void LoRa_beginPacket();
void LoRa_print(const char* str);
void LoRa_endPacket();
int LoRa_parsePacket();
int LoRa_available();
uint8_t LoRa_read(void);
#endif
