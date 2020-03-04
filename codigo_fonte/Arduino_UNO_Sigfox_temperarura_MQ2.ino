/* Projeto: detector de gases inflamáveis 
 *          e fumaça com SigFox e Tago.IO
 * Autor: Pedro Bertoleti 
 * Data: Fevereiro/2020
 */
#include <Wire.h> 
#include <SoftwareSerial.h>

/* Definições de compilação condicional */
#define ESCREVE_DEBUG_SERIAL_TEMP
#define ESCREVE_DEBUG_SERIAL_MQ2
#define ESCREVE_DEBUG_SERIAL_SIGFOX

/* Definições gerais */
#define HA_GASES_INFLAMAVEIS                   0x01
#define NAO_HA_GASES_INFLAMAVEIS               0x00
#define BAUDRATE_SERIAL_DEBUG                  115200
#define BAUDRATE_SERIAL_SIGFOX                 9600
#define GPIO_BREATHING_LIGHT                   13
#define GPIO_MQ2                               12
#define ANALOG_LM35                            A0
#define SIGFOX_SERIAL_RX_GPIO                  4
#define SIGFOX_SERIAL_TX_GPIO                  5
#define SIGFOX_MAX_FRAME_SIZE                  12
#define SIGFOX_VERIFICA_PRESENCA_MODEM         "AT\r"
#define SIGFOX_CMD_NUM_MICRO_MACRO_CANAIS      "AT$GI?\r"
#define SIGFOX_CMD_RESET_MICRO_MACRO_CANAIS    "AT$RC\r"
#define SIGFOX_CMD_ENVIO_DE_FRAME              "AT$SF="

/* Definições - máquina de estados do sensor de gás */
#define ESTADO_SENSOR_AGUARDA_ACIONAR          0x00
#define ESTADO_SENSOR_AGUARDA_DESACIONAR       0x01

/* Definições de temporização */
#define TEMPO_BREATHING_LIGHT_ON_OFF  250   //ms
#define TEMPO_ENTRE_MEDIDAS_SENSORES  500   //ms
#define TEMPO_MS_ENTRE_ENVIOS_SIGFOX  900000 //ms (= 15 minutos)
#define BURN_IN_TIME                  180   //s (= 3 minutos)
/* Typedefs */


typedef struct __attribute__((packed)) 
{
    char temperatura_ambiente;  //1 byte
    char mq2_gas_inflamavel;    //1 byte
    char reservado_1;            //1 byte
    char reservado_2;  //1 byte
    char reservado_3;  //1 byte    
    char reservado_4;  //1 byte
    char reservado_5;  //1 byte
    char reservado_6;  //1 byte
    char reservado_7;  //1 byte    
    char reservado_8;             //1 byte
    char reservado_9;             //1 byte
    char reservado_10;             //1 byte
                       // = 12 bytes
}TDados_Sigfox;

/* Variáveis e objetos globais */
SoftwareSerial sigfox(SIGFOX_SERIAL_RX_GPIO, SIGFOX_SERIAL_TX_GPIO); 
char estado_breathing_light = LOW;
bool gas_inflamavel_detectado = false;
unsigned long timestamp_envio_sigfox = 0;
unsigned long timestamp_breathing_light = 0;
unsigned long timestamp_leitura_sensores = 0;
char estado_sensor_gas = ESTADO_SENSOR_AGUARDA_ACIONAR;

/* Protótipos */
char le_temperatura(void);
char le_mq2(void);
void pisca_breathing_light(void);
unsigned long diferenca_tempo(unsigned long timestamp_referencia);
void init_sigfox(void);
String formata_frame_sigfox(unsigned char * ptr_data, uint8_t len);
void garante_macro_e_micro_canais(void);
void send_sigfox(char * ptr_data, uint8_t len);
void formata_e_envia_dados(void);
void verifica_sensor_gas(void);

/*
 * Implementações
 */

/* Função: inicializa comunicação com módulo Sigfox
 * Parâmetros: nenhum
 * Retorno: nenhum
 */
void init_sigfox(void)
{
    String status_presenca_frames = "";
    char output = "";

    /* Testa a presença do modem Sigfox */
    sigfox.print(SIGFOX_VERIFICA_PRESENCA_MODEM);

    while(sigfox.available())
    {
        output = (char)sigfox.read();
        status_presenca_frames += output;
        delay(10);
    }
      
    #ifdef ESCREVE_DEBUG_SERIAL_SIGFOX
    Serial.print("Resposta do ");
    Serial.print(SIGFOX_VERIFICA_PRESENCA_MODEM);
    Serial.print(": ");
    Serial.println(status_presenca_frames);    
    #endif

    Serial.println("\n ** Sigfox - setup OK **");
} 

/* Função: formata frame para ser enviado via Sigfox 
 *         (transforma array de bytes em uma Hex-String 
 *         para usar no comando AT de envio)
 * Parâmetros: ponteiro para array de dados e tamanho do array
 * Retorno: frame formatado (String)
 */
String formata_frame_sigfox(unsigned char * ptr_data, uint8_t len)
{
    String frame_sigfox = "";  
    uint8_t i;

    /* Se o tamanho do frame for menor que 12, faz padding */
    if (len < SIGFOX_MAX_FRAME_SIZE)
    {
        i = SIGFOX_MAX_FRAME_SIZE;
        while (i-- > len)
        {
            frame_sigfox += "00";
        }
    }

    /* Converte bytes para Hex-String */
    for(i = 0; i < len; i++) 
    {
        if (*ptr_data < 16) 
            frame_sigfox+="0";

        frame_sigfox += String(*ptr_data, HEX);
        ptr_data++;
    }

    Serial.print("Frame formatado: ");
    Serial.println(frame_sigfox);
  
    return frame_sigfox;
}

/* Função: garante que haja ao menos 3 micro canais utilizáveis 
 *         (condição para envio de frame vias Sigfox)
 * Parâmetros: nenhum
  * Retorno: nenhum
 */
void garante_macro_e_micro_canais(void)
{
    String status_num_micro_macro_canais = "";
    String status_reset_num_micro_macro_canais = "";
    int macro_canais = 0;
    int micro_canais = 0;
    char output = "";
    int idx_virgula = 0;

    sigfox.print(SIGFOX_CMD_NUM_MICRO_MACRO_CANAIS);
    while(sigfox.available())
    {
        output = (char)sigfox.read();
        status_num_micro_macro_canais += output;
        delay(10);
    }

    #ifdef ESCREVE_DEBUG_SERIAL_SIGFOX
    Serial.print("Resposta do ");
    Serial.print(SIGFOX_CMD_NUM_MICRO_MACRO_CANAIS);
    Serial.print(":");
    Serial.println(status_num_micro_macro_canais);
    #endif

    /* Obtem numero de micro e macro canais */
    idx_virgula = status_num_micro_macro_canais.indexOf(",");
    macro_canais = status_num_micro_macro_canais.charAt(idx_virgula-1) - 0x30;
    micro_canais = status_num_micro_macro_canais.charAt(idx_virgula+1) - 0x30;

    /* Verifica se é preciso fazer reset dos micro e macro canais */
    if ( (macro_canais == 0) || (micro_canais < 3) )
    {
        /* Menos de 3 micro canais disponíveis. É necessário fazer o reset */  
        #ifdef ESCREVE_DEBUG_SERIAL_SIGFOX
        Serial.println("Menos de 3 micro canais disponíveis. O reset dos micro e macro canais sera feito.");
        #endif  

        status_reset_num_micro_macro_canais = "";
        sigfox.print(SIGFOX_CMD_RESET_MICRO_MACRO_CANAIS);
        
        while(sigfox.available())
        {
            output = (char)sigfox.read();
            status_reset_num_micro_macro_canais += output;
            delay(10);
        }
    
        #ifdef ESCREVE_DEBUG_SERIAL_SIGFOX
        Serial.print("Resposta do ");
        Serial.print(SIGFOX_CMD_RESET_MICRO_MACRO_CANAIS);
        Serial.print(":");
        Serial.println(status_reset_num_micro_macro_canais);
        #endif
    }
}

/* Função: formata frame e o enviapara Sigfox Cloud
 * Parâmetros: ponteiro para array de dados e tamanho do array. 
  * Retorno: nenhum
 */
void send_sigfox(char * ptr_data, uint8_t len)
{
    String frame_sigfox = "";
    String status_envio_de_frame = "";
    char output = "";

    /* Formata o frame para ser enviado */
    frame_sigfox = formata_frame_sigfox(ptr_data, len);
    
    /* Garante número de macro e micro canais necessários */
    garante_macro_e_micro_canais();

    /* Faz o envio do frame */
    status_envio_de_frame = "";
    sigfox.print(SIGFOX_CMD_ENVIO_DE_FRAME);
    sigfox.print(frame_sigfox);
    sigfox.print("\r");
    while (!sigfox.available());

    status_envio_de_frame = "";
    while(sigfox.available())
    {
        output = (char)sigfox.read();
        status_envio_de_frame += output;
        delay(10);
    }

    #ifdef ESCREVE_DEBUG_SERIAL_SIGFOX
    Serial.print("Resposta do ");
    Serial.print(SIGFOX_CMD_ENVIO_DE_FRAME);
    Serial.print(":");
    Serial.println(status_envio_de_frame);
    #endif
}

/* Função: formata e envia dados para Sigflox Cloud
 * Parâmetros: nenhum
 * Retorno: nenhum
 */
void formata_e_envia_dados(void)
{
    TDados_Sigfox dados_sigfox;
        
    /* Empacota aceleração em pacote de 12 bytes e envia para Sigfox */
    memset((unsigned char *)&dados_sigfox, 0, sizeof(TDados_Sigfox));
    dados_sigfox.temperatura_ambiente = le_temperatura();
    dados_sigfox.mq2_gas_inflamavel = le_mq2();

    /* Faz envio para Sigfox Cloud */
    send_sigfox((unsigned char *)&dados_sigfox, sizeof(TDados_Sigfox));
}

/* Função: le temperatura do LM35 (graus Celsius)
 * Parâmetros: nenhum
 * Retorno: nenhum
 */
char le_temperatura(void)
{
    float temperatura_lida = 0.0;
    char temp_1byte = 0x00;

    temperatura_lida = (float(analogRead(ANALOG_LM35))*5/(1023))/0.01;
    temp_1byte = (char)temperatura_lida;
    
    /* Escreve as acelerações lidas na serial de debug */
    #ifdef ESCREVE_DEBUG_SERIAL_TEMP
    Serial.print("Temperatura: ");
    Serial.print(temperatura_lida);
    Serial.println("C"); 
    #endif

    return temp_1byte;
}


/* Função: le saída digital do MQ-2 (sensor de gases inflamáveis)
 * Parâmetros: nenhum
 * Retorno: NAO_HA_GASES_INFLAMAVEIS ou HA_GASES_INFLAMAVEIS
 */
char le_mq2(void)
{
    char leitura = NAO_HA_GASES_INFLAMAVEIS;
    
    if (digitalRead(GPIO_MQ2) == LOW)
        leitura = HA_GASES_INFLAMAVEIS;

    return leitura;  
}

/* Função: calcula diferença de tempo (ms) entre o timestamp atual e
 *         uma referência
 * Parâmetros: timestamp de referência
 * Retorno: diferença de tempo (ms)
 */
unsigned long diferenca_tempo(unsigned long timestamp_referencia)
{
    unsigned long diferenca_tempo;

    diferenca_tempo = millis() - timestamp_referencia;
    return diferenca_tempo;
}

/* Função: pisca o breathing light
 * Parâmetros: nenhum
 * Retorno: nenhum
 */
void pisca_breathing_light(void)
{
    /* Muda estado do breathing light */
    if (estado_breathing_light == LOW)
        estado_breathing_light = HIGH;  
    else
        estado_breathing_light = LOW;

    digitalWrite(GPIO_BREATHING_LIGHT, estado_breathing_light);
}

/* Função: verifica se o sensor de gás inflamável e fumaça está acionado
 * Parâmetros: nenhum
 * Retorno: nenhum
 */
void verifica_sensor_gas(void)
{ 
    switch (estado_sensor_gas)   
    {
        case ESTADO_SENSOR_AGUARDA_ACIONAR:
            if (le_mq2() == HA_GASES_INFLAMAVEIS)
            {              
                estado_sensor_gas = ESTADO_SENSOR_AGUARDA_DESACIONAR;
                gas_inflamavel_detectado = true;

                #ifdef ESCREVE_DEBUG_SERIAL_MQ2
                Serial.println("* Gases inflamaveis e/ou fumaca DETECTADOS!");
                #endif                
            }
            break;
  
        case ESTADO_SENSOR_AGUARDA_DESACIONAR:
            if (le_mq2() == NAO_HA_GASES_INFLAMAVEIS)
            {
                estado_sensor_gas = ESTADO_SENSOR_AGUARDA_ACIONAR;

                #ifdef ESCREVE_DEBUG_SERIAL_MQ2
                Serial.println("* Gases inflamaveis e/ou fumaca nao detectados!");
                #endif            
            }
            break; 

       default: 
           break;     
    }
}

void setup() 
{
    int burn_in_time = BURN_IN_TIME;
    
    /* Inicializa serial e serial para comunicar
       com módulo Sigfox */
    Serial.begin(BAUDRATE_SERIAL_DEBUG);
    sigfox.begin(BAUDRATE_SERIAL_SIGFOX);
    
    /* Inicializa e configura GPIO do breathing light */ 
    pinMode(GPIO_BREATHING_LIGHT, OUTPUT);
    estado_breathing_light = LOW;
    digitalWrite(GPIO_BREATHING_LIGHT, estado_breathing_light);

    /* Inicializa GPIO do sensor MQ-2 */
    pinMode(GPIO_MQ2, INPUT);

    /* Aguarda o burn in time (tempo de queima do MQ-2) */
    while(burn_in_time > 0)
    {
        Serial.print("* ");
        Serial.print(burn_in_time);
        Serial.println("s para o fim do burn-in time");
        burn_in_time--;
        pisca_breathing_light();        
        delay(1000);
    }

    /* Inicializa máquina de estados do sensor de gás e fumaça */
    estado_sensor_gas = ESTADO_SENSOR_AGUARDA_ACIONAR;
    
    /* Inicializa comunicação com módulo Sigfox */
    init_sigfox();

    /* Inicializa variáveis de temporização */
    timestamp_envio_sigfox = millis();
    timestamp_breathing_light = millis();
    timestamp_leitura_sensores = millis();    
}

void loop() 
{       
    /* verifica se sensor de gás e fumaça foi acionado */
    verifica_sensor_gas();

    /* Verifica se é a hora de piscar o breathing light */     
    if (diferenca_tempo(timestamp_breathing_light) >= TEMPO_BREATHING_LIGHT_ON_OFF)
    {
        /* Pisca o breathing light */
        pisca_breathing_light();
        
        /* Reinicia referência de tempo para piscar breathing light */
        timestamp_breathing_light = millis();
    }

    /* Verifica se é a hora de ler os sensores MQ-2 e LM35 */     
    if (diferenca_tempo(timestamp_leitura_sensores) >= TEMPO_ENTRE_MEDIDAS_SENSORES)
    {
        /* Faz a leitura do termometro */
        le_temperatura(); 

        /* Faz a leitura do sensor de gases inflamáveis */
        le_mq2();

        /* Reinicia referência de tempo para ler acelerometro */
        timestamp_leitura_sensores = millis();        
    }

    /* Verifica se é a hora de enviar informações para Sigfox */     
    if ( (gas_inflamavel_detectado == true) || (diferenca_tempo(timestamp_envio_sigfox) >= TEMPO_MS_ENTRE_ENVIOS_SIGFOX) )
    {
        /* Faz a formatação dos daos e os envia para SigFox Cloud */
        formata_e_envia_dados();

        /* Caso o envio foi forçado devido a presença de gás inflamável, limpa o flag que forçou o envio */
        if (gas_inflamavel_detectado == true)
            gas_inflamavel_detectado = false;
        
        /* Reinicia referência de tempo para enviar informações ao Sigfox */
        timestamp_envio_sigfox = millis(); 
    }
}
