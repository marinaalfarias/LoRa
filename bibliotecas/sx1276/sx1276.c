#include "sx1276.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

static spi_inst_t* _spi;
static uint _cs;
static uint _rst;

static void write_reg(uint8_t addr, uint8_t value) {
    gpio_put(_cs, 0);
    uint8_t buf[2] = { (uint8_t)(addr | 0x80), value };
    spi_write_blocking(_spi, buf, 2);
    gpio_put(_cs, 1);
}

void sx1276_init(spi_inst_t *spi, uint cs, uint rst) {
    _spi = spi;
    _cs = cs;
    _rst = rst;

    gpio_put(_rst, 0);
    sleep_ms(100);
    gpio_put(_rst, 1);
    sleep_ms(100);

    write_reg(0x01, 0x80); // Modo LoRa + Sleep
    sleep_ms(10);
    write_reg(0x01, 0x81); // Modo LoRa + Standby
}

void sx1276_set_frequency(uint32_t freq_hz) {
    uint64_t frf = ((uint64_t)freq_hz << 19) / 32000000;
    write_reg(0x06, (uint8_t)(frf >> 16));
    write_reg(0x07, (uint8_t)(frf >> 8));
    write_reg(0x08, (uint8_t)(frf >> 0));
}

void sx1276_set_tx_power(int8_t power) {
    if (power < 2) power = 2;
    if (power > 17) power = 17;
    write_reg(0x09, 0x80 | (power - 2));
}

void sx1276_send_packet(uint8_t* data, uint8_t len) {
    write_reg(0x0D, 0x00); // FIFO TX base addr
    for (uint8_t i = 0; i < len; i++) {
        write_reg(0x00, data[i]); // FIFO
    }
    write_reg(0x22, len);      // Payload length
    write_reg(0x01, 0x83);     // Modo Transmitir
    sleep_ms(200);             // Aguarda envio (pode melhorar com DIO0)
    write_reg(0x01, 0x81);     // Volta pro standby
}
