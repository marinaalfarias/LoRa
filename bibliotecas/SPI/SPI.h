#ifndef SPI_H
#define SPI_H

#include <stdint.h>

void SPI_init();
uint8_t SPI_transfer(uint8_t data);

#endif
