# Detector de gases inflamáveis e fumaça com SigFox e Tago.IO

Este é o repositório oficial do projeto detector de gases inflamáveis e fumaça com SigFox e Tago.IO, sendo um projeto totalmente open-source. Aqui você encontra todos os arquivos (códigos-fonte e circuitos esquemáticos) referentes ao projeto.
Este trata-se de um projeto no contexto de Internet das Coisas, com finalidade de monitorar e emitir alertas de detecção de gases inflamáveis e fumaça em quaisquer ambientes, usando como LPWAN o SigFox (https://www.sigfox.com/en) e como plataforma IoT o Tago.IO (https://tago.io/).

# Software embarcado

O software embarcado do projeto é responsável pelas seguintes ações:

* De 15 em 15 minutos, fará a leitura da temperatura ambiente (sensor: LM35) e enviará este dado para a SigFox Cloud
* De 15 em 15 minutos, fará a leitura do sensor de gases inflamáveis e fumaça (MQ-2) e enviará este dado para a SigFox Cloud
* A qualquer momento, se houver adetecção de gases inflamáveis e fumaça, em carater emergencial, imediatamente é feito o envio de  um pacote de dados contendo a temperatura ambiente e byte indicando a detecção de gases inflamáveis e/ou fumaça.
Será considerado o LED ligado ao GPIO 13 como um breathing light, piscando sempre para indicar que o dispositivo embarcado está operando.

# Hardware utilizado

O projeto utiliza como hardware as seguintes partes:
 
* Arduino UNO R3 (https://www.filipeflop.com/produto/placa-uno-r3-cabo-usb-para-arduino/)
* Shield IoT Tatamaya Black

![Shield IoT Tatamaya Black](https://github.com/phfbertoleti/detector_gas_inflamavel_sigfox_tagoio/blob/master/hardware_imagens/tatamaya_black.jpg)

* SigBOT: Placa de comunicação SigFox

![SigBOT](https://github.com/phfbertoleti/detector_gas_inflamavel_sigfox_tagoio/blob/master/hardware_imagens/sigbot.jpg)

Para adquirir a Tatamaya Black e a placa SigBOT, entre em contato com Cirineu C. Fernandes pelo e-mail sirineotechnologies.adm@gmail.com ou telefone (61) 99865-4343
