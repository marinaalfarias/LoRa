# lorawan.c
Esse arquivo é responsável por implementar as funcionalidades do protocolo LoRaWAN no dispositivo. Dentro do arquivo, temos:

- Inicialização do LoRaWAN (lorawan_init()) que inicializa o sistema, configurando parâmetros como: sessions keys, DevAddr, etc;
- Gerenciamento do envio de pacotes (lorawan_send_unconfirmed() e lorawan_send_confirmed()) que são resposáveis por confirmar o envio de pacote de dados
- Processamento de eventos e mensagens;
- Funções auxiliares e de confirmação.
