#include "LoRa.h"
#include "SPI.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define NSS 17
#define RESET 21
#define DIO0 20

#define REG_FIFO                    0x00
#define REG_OP_MODE                0x01
#define REG_FRF_MSB                0x06
#define REG_FRF_MID                0x07
#define REG_FRF_LSB                0x08
#define REG_PA_CONFIG              0x09
#define REG_FIFO_ADDR_PTR          0x0D
#define REG_PAYLOAD_LENGTH         0x22
#define REG_FIFO_TX_BASE_ADDR      0x0E
#define REG_FIFO_RX_BASE_ADDR      0x0F
#define REG_IRQ_FLAGS              0x12
#define REG_RX_NB_BYTES            0x13
#define REG_FIFO_RX_CURRENT_ADDR   0x10

#define MODE_LONG_RANGE_MODE       0x80
#define MODE_TX                    0x83
#define MODE_RX_CONTINUOUS         0x85
#define MODE_SLEEP                 0x00
#define MODE_STDBY                 0x01

#define PA_BOOST                   0x80

static void writeRegister(uint8_t reg, uint8_t value) {
    gpio_put(NSS, 0);
    SPI_transfer(reg | 0x80);
    SPI_transfer(value);
    gpio_put(NSS, 1);
}

static uint8_t readRegister(uint8_t reg) {
    gpio_put(NSS, 0);
    SPI_transfer(reg & 0x7F);
    uint8_t value = SPI_transfer(0x00);
    gpio_put(NSS, 1);
    return value;
}

void LoRa_init() {
    gpio_init(NSS);
    gpio_set_dir(NSS, GPIO_OUT);
    gpio_put(NSS, 1);

    gpio_init(RESET);
    gpio_set_dir(RESET, GPIO_OUT);
    gpio_put(RESET, 0);
    sleep_ms(10);
    gpio_put(RESET, 1);
    sleep_ms(10);

    SPI_init();

    writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
    writeRegister(REG_FIFO_TX_BASE_ADDR, 0);
    writeRegister(REG_FIFO_RX_BASE_ADDR, 0);
    writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

void LoRa_setFrequency(long frequency) {
    long frf = (frequency << 19) / 32000000;
    writeRegister(REG_FRF_MSB, (uint8_t)(frf >> 16));
    writeRegister(REG_FRF_MID, (uint8_t)(frf >> 8));
    writeRegister(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

void LoRa_setTxPower(int level, int useRfo) {
    if (useRfo) {
        if (level < 0) level = 0;
        if (level > 14) level = 14;
        writeRegister(REG_PA_CONFIG, 0x70 | level);
    } else {
        if (level > 17) level = 17;
        writeRegister(REG_PA_CONFIG, PA_BOOST | (level - 2));
    }
}

void LoRa_beginPacket() {
    writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
    writeRegister(REG_FIFO_ADDR_PTR, 0);
}

void LoRa_write(uint8_t byte) {
    writeRegister(REG_FIFO, byte);
}

void LoRa_print(const char *str) {
    while (*str) {
        LoRa_write((uint8_t)(*str));
        str++;
    }
}

void LoRa_endPacket() {
    writeRegister(REG_PAYLOAD_LENGTH, 0); // deixar 0 = tamanho automático
    writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);
    while ((readRegister(REG_IRQ_FLAGS) & 0x08) == 0) {
        tight_loop_contents(); // aguarda envio
    }
    writeRegister(REG_IRQ_FLAGS, 0x08); // limpa flag
}

int LoRa_parsePacket() {
    uint8_t irqFlags = readRegister(REG_IRQ_FLAGS);
    if ((irqFlags & 0x40) == 0) return 0;
    writeRegister(REG_IRQ_FLAGS, 0x40);

    int packetLength = readRegister(REG_RX_NB_BYTES);
    writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_CURRENT_ADDR));
    return packetLength;
}

int LoRa_available() {
    return readRegister(REG_RX_NB_BYTES);
}

uint8_t LoRa_read() {
    return readRegister(REG_FIFO);
}
