#include "SPI.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

void SPI_init() {
    spi_init(SPI_PORT, 500 * 1000);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
}

uint8_t SPI_transfer(uint8_t data) {
    uint8_t rx;
    spi_write_read_blocking(SPI_PORT, &data, &rx, 1);
    return rx;
}
