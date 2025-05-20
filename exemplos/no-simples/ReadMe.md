# Nó Simples
Esse código ilustra a configuração e a utilização da biblioteca ``pico-lora`` para enviar e receber mensagens via comunicação LoRa. Serve como **ponto de partida** para projetos que necessitam de comunicação sem fio de longa distância e baixo consumo de energia

## Funcionamento do Código
O código configura o microcontrolador para operar com um **nó LoRa simples**. Dependendo da configuração, o dispositivo pode atuar como transmissou ou receptor.

Nó código, os pinos estão configurados da seguinte forma
- SCK: GPIO 18
- MOSI: GPIO 19
- MISO: GPIO 16
- NSS (CS): GPIO 8
- RESET: GPIO 9
- DIO0: GPIO

Caso precise, os pinos podem ser reconfigurados como necessário utilizando a função ``setPins()``.

## Considerações Adicionais
Esse exemplo é baseada na biblioteca ``pico-lora`` que fornece uma interface simplificada para comunicação LoRa em dispositivos RP2040. Para isso, é necessário clonar o repositório ``pico-lora`` e compilar o código utilizando as ferramentas fornecidas pelo SDK do Pico.
