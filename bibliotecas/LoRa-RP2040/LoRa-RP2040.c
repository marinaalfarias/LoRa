#include "LoRa-RP2040.h"
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

// registers
#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FRF_MSB              0x06
#define REG_FRF_MID              0x07
#define REG_FRF_LSB              0x08
#define REG_PA_CONFIG            0x09
#define IRQ_CAD_DETECTED_MASK    0x01
#define REG_OCP                  0x0b
#define REG_LNA                  0x0c
#define IRQ_CAD_DONE_MASK        0x04
#define REG_FIFO_ADDR_PTR        0x0d
#define REG_FIFO_TX_BASE_ADDR    0x0e
#define REG_FIFO_RX_BASE_ADDR    0x0f
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS            0x12
#define REG_RX_NB_BYTES          0x13
#define REG_PKT_SNR_VALUE        0x19
#define REG_PKT_RSSI_VALUE       0x1a
#define REG_RSSI_VALUE           0x1b
#define REG_MODEM_CONFIG_1       0x1d
#define REG_MODEM_CONFIG_2       0x1e
#define REG_PREAMBLE_MSB         0x20
#define REG_PREAMBLE_LSB         0x21
#define REG_PAYLOAD_LENGTH       0x22
#define REG_MODEM_CONFIG_3       0x26
#define REG_FREQ_ERROR_MSB       0x28
#define REG_FREQ_ERROR_MID       0x29
#define REG_FREQ_ERROR_LSB       0x2a
#define REG_RSSI_WIDEBAND        0x2c
#define REG_DETECTION_OPTIMIZE   0x31
#define REG_INVERTIQ             0x33
#define REG_DETECTION_THRESHOLD  0x37
#define REG_SYNC_WORD            0x39
#define REG_INVERTIQ2            0x3b
#define REG_DIO_MAPPING_1        0x40
#define REG_VERSION              0x42
#define REG_PA_DAC               0x4d
#define MODE_CAD                 0x07
// modes
#define MODE_LONG_RANGE_MODE     0x80
#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_TX                  0x03
#define MODE_RX_CONTINUOUS       0x05
#define MODE_RX_SINGLE           0x06

// PA config
#define PA_BOOST                 0x80

// IRQ masks
#define IRQ_TX_DONE_MASK           0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
#define IRQ_RX_DONE_MASK           0x40

#define RF_MID_BAND_THRESHOLD    525E6
#define RSSI_OFFSET_HF_PORT      157
#define RSSI_OFFSET_LF_PORT      164

#define MAX_PKT_LENGTH           255

#if (ESP8266 || ESP32)
#define ISR_PREFIX ICACHE_RAM_ATTR
#else
#define ISR_PREFIX
#endif
// Estrutura para substituir a classe LoRaClass
typedef struct {
    spi_inst_t* spi;
    int ss;
    int reset;
    int dio0;
    long frequency;
    int packet_index;
    int implicit_header_mode;
    void (*on_receive)(int);
    void (*on_cad_done)(bool);
    void (*on_tx_done)(void);
} LoRa;

// Instância global (substitui o singleton original)
static LoRa lora_instance;

// Protótipos de funções internas
static uint8_t read_register(uint8_t address);
static void write_register(uint8_t address, uint8_t value);
static uint8_t single_transfer(uint8_t address, uint8_t value);
static void handle_dio0_rise(void);

// Implementações

void LoRa_init(void) {
    lora_instance.spi = spi0;
    lora_instance.ss = LORA_DEFAULT_SS_PIN;
    lora_instance.reset = LORA_DEFAULT_RESET_PIN;
    lora_instance.dio0 = LORA_DEFAULT_DIO0_PIN;
    lora_instance.frequency = 0;
    lora_instance.packet_index = 0;
    lora_instance.implicit_header_mode = 0;
    lora_instance.on_receive = NULL;
    lora_instance.on_cad_done = NULL;
    lora_instance.on_tx_done = NULL;
}

int LoRa_begin(long frequency) {
    // Configuração inicial dos pinos
    gpio_init(lora_instance.ss);
    gpio_set_dir(lora_instance.ss, GPIO_OUT);
    gpio_put(lora_instance.ss, 1);

    if (lora_instance.reset != -1) {
        gpio_init(lora_instance.reset);
        gpio_set_dir(lora_instance.reset, GPIO_OUT);
        gpio_put(lora_instance.reset, 0);
        sleep_ms(10);
        gpio_put(lora_instance.reset, 1);
        sleep_ms(10);
    }

    // Inicialização do SPI
    spi_init(spi0, 12500 * 1000); // Ajuste conforme necessário
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // Verificação de versão
    uint8_t version = read_register(REG_VERSION);
    if (version != 0x12) return 0;

    // Configurações iniciais (similar ao original)
    LoRa_sleep();
    LoRa_set_frequency(frequency);
    write_register(REG_FIFO_TX_BASE_ADDR, 0);
    write_register(REG_FIFO_RX_BASE_ADDR, 0);
    write_register(REG_LNA, read_register(REG_LNA) | 0x03);
    write_register(REG_MODEM_CONFIG_3, 0x04);
    LoRa_set_tx_power(17);
    LoRa_idle();
    
    return 1;
}

// Continuação das implementações...

int LoRa_begin_packet(int implicit_header) {
    if (LoRa_is_transmitting()) return 0;

    LoRa_idle();

    if (implicit_header) {
        LoRa_implicit_header_mode();
    } else {
        LoRa_explicit_header_mode();
    }

    write_register(REG_FIFO_ADDR_PTR, 0);
    write_register(REG_PAYLOAD_LENGTH, 0);
    return 1;
}

int LoRa_end_packet(bool async) {
    if (async && lora_instance.on_tx_done) {
        write_register(REG_DIO_MAPPING_1, 0x40);
    }

    write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

    if (!async) {
        while ((read_register(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) == 0) {
            sleep_ms(0);
        }
        write_register(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
    }
    return 1;
}

bool LoRa_is_transmitting(void) {
    if ((read_register(REG_OP_MODE) & MODE_TX) == MODE_TX) return true;
    
    if (read_register(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) {
        write_register(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
    }
    return false;
}

int LoRa_parse_packet(int size) {
    int packet_length = 0;
    uint8_t irq_flags = read_register(REG_IRQ_FLAGS);

    if (size > 0) {
        LoRa_implicit_header_mode();
        write_register(REG_PAYLOAD_LENGTH, size & 0xff);
    } else {
        LoRa_explicit_header_mode();
    }

    write_register(REG_IRQ_FLAGS, irq_flags);

    if ((irq_flags & IRQ_RX_DONE_MASK) && !(irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK)) {
        lora_instance.packet_index = 0;
        packet_length = lora_instance.implicit_header_mode ? 
            read_register(REG_PAYLOAD_LENGTH) : 
            read_register(REG_RX_NB_BYTES);
        
        write_register(REG_FIFO_ADDR_PTR, read_register(REG_FIFO_RX_CURRENT_ADDR));
        LoRa_idle();
    } else if (read_register(REG_OP_MODE) != (MODE_LONG_RANGE_MODE | MODE_RX_SINGLE)) {
        write_register(REG_FIFO_ADDR_PTR, 0);
        write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_SINGLE);
    }
    return packet_length;
}

int LoRa_packet_rssi(void) {
    return read_register(REG_PKT_RSSI_VALUE) - 
        (lora_instance.frequency < RF_MID_BAND_THRESHOLD ? 
         RSSI_OFFSET_LF_PORT : RSSI_OFFSET_HF_PORT);
}

float LoRa_packet_snr(void) {
    return ((int8_t)read_register(REG_PKT_SNR_VALUE)) * 0.25f;
}

size_t LoRa_write(const uint8_t *buffer, size_t size) {
    int current_length = read_register(REG_PAYLOAD_LENGTH);
    
    if ((current_length + size) > MAX_PKT_LENGTH) {
        size = MAX_PKT_LENGTH - current_length;
    }

    for (size_t i = 0; i < size; i++) {
        write_register(REG_FIFO, buffer[i]);
    }

    write_register(REG_PAYLOAD_LENGTH, current_length + size);
    return size;
}

int LoRa_available(void) {
    return read_register(REG_RX_NB_BYTES) - lora_instance.packet_index;
}

int LoRa_read(void) {
    if (!LoRa_available()) return -1;
    lora_instance.packet_index++;
    return read_register(REG_FIFO);
}

void LoRa_receive(int size) {
    write_register(REG_DIO_MAPPING_1, 0x00);

    if (size > 0) {
        LoRa_implicit_header_mode();
        write_register(REG_PAYLOAD_LENGTH, size & 0xff);
    } else {
        LoRa_explicit_header_mode();
    }
    write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

void LoRa_idle(void) {
    write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

void LoRa_sleep(void) {
    write_register(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

void LoRa_set_tx_power(int level) {
    if (level > 17) {
        level = (level > 20) ? 20 : level;
        level -= 3;
        write_register(REG_PA_DAC, 0x87);
        LoRa_set_ocp(140);
    } else {
        level = (level < 2) ? 2 : level;
        write_register(REG_PA_DAC, 0x84);
        LoRa_set_ocp(100);
    }
    write_register(REG_PA_CONFIG, PA_BOOST | (level - 2));
}

static void LoRa_set_ocp(uint8_t ma) {
    uint8_t ocp = 27;
    if (ma <= 120) {
        ocp = (ma - 45) / 5;
    } else if (ma <= 240) {
        ocp = (ma + 30) / 10;
    }
    write_register(REG_OCP, 0x20 | (ocp & 0x1F));
}

void LoRa_set_sync_word(int sw) {
    write_register(REG_SYNC_WORD, sw);
}

void LoRa_enable_crc(void) {
    write_register(REG_MODEM_CONFIG_2, read_register(REG_MODEM_CONFIG_2) | 0x04);
}

void LoRa_disable_crc(void) {
    write_register(REG_MODEM_CONFIG_2, read_register(REG_MODEM_CONFIG_2) & ~0x04);
}

// Continuação das funções de configuração do modem...

void LoRa_set_spreading_factor(int sf) {
    if (sf < 6) sf = 6;
    else if (sf > 12) sf = 12;

    if (sf == 6) {
        write_register(REG_DETECTION_OPTIMIZE, 0xc5);
        write_register(REG_DETECTION_THRESHOLD, 0x0c);
    } else {
        write_register(REG_DETECTION_OPTIMIZE, 0xc3);
        write_register(REG_DETECTION_THRESHOLD, 0x0a);
    }

    write_register(REG_MODEM_CONFIG_2, 
        (read_register(REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
    
    LoRa_set_ldo_flag();
}

static void LoRa_set_ldo_flag(void) {
    long bw = LoRa_get_signal_bandwidth();
    long symbol_duration = 1000 / (bw / (1L << LoRa_get_spreading_factor()));
    
    bool ldo_on = symbol_duration > 16;
    uint8_t config3 = read_register(REG_MODEM_CONFIG_3);
    
    config3 = ldo_on ? 
        (config3 | (1 << 3)) : 
        (config3 & ~(1 << 3));
    
    write_register(REG_MODEM_CONFIG_3, config3);
}

// ... (demais funções de configuração seguindo o mesmo padrão)

static void handle_dio0_rise(void) {
    uint8_t irq_flags = read_register(REG_IRQ_FLAGS);
    write_register(REG_IRQ_FLAGS, irq_flags); // Limpa flags

    if (irq_flags & IRQ_CAD_DONE_MASK) {
        if (lora_instance.on_cad_done) {
            lora_instance.on_cad_done(irq_flags & IRQ_CAD_DETECTED_MASK);
        }
    } else if (!(irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK)) {
        if (irq_flags & IRQ_RX_DONE_MASK) {
            lora_instance.packet_index = 0;
            int packet_length = lora_instance.implicit_header_mode ? 
                read_register(REG_PAYLOAD_LENGTH) : 
                read_register(REG_RX_NB_BYTES);
            write_register(REG_FIFO_ADDR_PTR, read_register(REG_FIFO_RX_CURRENT_ADDR));
            if (lora_instance.on_receive) {
                lora_instance.on_receive(packet_length);
            }
        } else if (irq_flags & IRQ_TX_DONE_MASK) {
            if (lora_instance.on_tx_done) {
                lora_instance.on_tx_done();
            }
        }
    }
}

// Função de callback para interrupção GPIO
static void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == lora_instance.dio0) {
        handle_dio0_rise();
    }
}

void LoRa_set_on_receive(void (*callback)(int)) {
    lora_instance.on_receive = callback;
    if (callback) {
        gpio_set_irq_enabled_with_callback(lora_instance.dio0, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    } else {
        gpio_set_irq_enabled(lora_instance.dio0, GPIO_IRQ_EDGE_RISE, false);
    }
}

void LoRa_set_frequency(long frequency) {
    lora_instance.frequency = frequency;
    uint64_t frf = ((uint64_t)frequency << 19) / 32000000;
    write_register(REG_FRF_MSB, (frf >> 16) & 0xFF);
    write_register(REG_FRF_MID, (frf >> 8) & 0xFF);
    write_register(REG_FRF_LSB, frf & 0xFF);
}

// Funções restantes:

void LoRa_explicit_header_mode(void) {
    lora_instance.implicit_header_mode = 0;
    write_register(REG_MODEM_CONFIG_1, read_register(REG_MODEM_CONFIG_1) & ~0x01);
}

void LoRa_implicit_header_mode(void) {
    lora_instance.implicit_header_mode = 1;
    write_register(REG_MODEM_CONFIG_1, read_register(REG_MODEM_CONFIG_1) | 0x01);
}

void LoRa_set_coding_rate4(int denominator) {
    int cr = (denominator < 5) ? 1 : (denominator > 8) ? 4 : (denominator - 4);
    write_register(REG_MODEM_CONFIG_1, 
        (read_register(REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
}

void LoRa_set_preamble_length(long length) {
    write_register(REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
    write_register(REG_PREAMBLE_LSB, (uint8_t)(length & 0xFF));
}

int LoRa_get_spreading_factor(void) {
    return read_register(REG_MODEM_CONFIG_2) >> 4;
}

long LoRa_get_signal_bandwidth(void) {
    uint8_t bw = (read_register(REG_MODEM_CONFIG_1) >> 4);
    const long bw_table[] = {7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000};
    return (bw < 10) ? bw_table[bw] : -1;
}

void LoRa_set_signal_bandwidth(long sbw) {
    int bw;
    const long thresholds[] = {7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000};
    for (bw = 9; bw > 0 && sbw <= thresholds[bw]; bw--);
    write_register(REG_MODEM_CONFIG_1, 
        (read_register(REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
    LoRa_set_ldo_flag();
}

void LoRa_enable_invert_iq(void) {
    write_register(REG_INVERTIQ, 0x66);
    write_register(REG_INVERTIQ2, 0x19);
}

void LoRa_disable_invert_iq(void) {
    write_register(REG_INVERTIQ, 0x27);
    write_register(REG_INVERTIQ2, 0x1d);
}

// Funções de acesso ao hardware
static uint8_t single_transfer(uint8_t address, uint8_t value) {
    uint8_t response;
    gpio_put(lora_instance.ss, 0);
    spi_write_blocking(lora_instance.spi, &address, 1);
    spi_write_read_blocking(lora_instance.spi, &value, &response, 1);
    gpio_put(lora_instance.ss, 1);
    return response;
}

static uint8_t read_register(uint8_t address) {
    return single_transfer(address & 0x7f, 0x00);
}

static void write_register(uint8_t address, uint8_t value) {
    single_transfer(address | 0x80, value);
}
