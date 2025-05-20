#ifndef SX1276_H
#define SX1276_H

#include "hardware/spi.h"

void sx1276_init(spi_inst_t *spi, uint cs, uint rst);
void sx1276_set_frequency(uint32_t freq_hz);
void sx1276_set_tx_power(int8_t power);
void sx1276_send_packet(uint8_t* data, uint8_t len);

#endif
