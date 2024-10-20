#include <LkRadioStructure.h>

const int LED_PIN = 13;
const int RECEIVE_PIN = 11;

#define SANITARIA_calda 0
#define SANITARIA_fredda 4
#define TERMO_calda 2
#define TERMO_fredda 3
#define AMBIENTE 1

#define T_SALE 2
#define T_SCENDE 0
#define T_COSTANTE 1

// float temperatura = 25.37;  // Temperatura da inviare
// int16_t temperaturaCodificata = (int16_t)(temperatura * 100);


struct __attribute__((packed)) packet_contatori {
    byte sender; // 1 byte
    uint16_t sanitariaCount; // 2 bytes
    uint16_t termoCount; // 2 bytes
};

struct __attribute__((packed)) packet_sanitaria_termo {
    byte sender; // 1 byte
    byte stato; // 1byte 0=discesa 1=costante 2=salita
    int16_t temperaturaCaldaC; // 2byte
};

struct __attribute__((packed)) packet_ambiente {
    byte sender; // 1 byte
    int16_t temperaturaC; // 2 byte
};

LkRadioStructure<packet_contatori> radio_contatori;
LkRadioStructure<packet_sanitaria_termo> radio_sanitaria;
LkRadioStructure<packet_ambiente> radio_ambiente;

void setup() {
    pinMode(LED_PIN, OUTPUT);
    //
    Serial.begin(115200);
    while (!Serial) {
        ; // aspetta la connessione del porto seriale
    }
	Serial.println();    
    Serial.println("setup");
    //
    radio_contatori.globalSetup(2000, -1, RECEIVE_PIN);  // Solo ricezione
}

void loop() {
    if (radio_contatori.haveRawMessage()) {
        uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
        uint8_t rawBufferLen;
        radio_contatori.getRawBuffer(rawBuffer, rawBufferLen);
        if (rawBufferLen == 5) {
            // Deserializza il messaggio nel tipo corretto di struttura
            LkArraylize<packet_contatori> converter;
            packet_contatori ds_contatori = converter.deArraylize(rawBuffer);
            if (ds_contatori.sender == 99){
                Serial.println();
                Serial.print("Sanitaria count: ");
                Serial.println(ds_contatori.sanitariaCount);
                Serial.print("Termo count:     ");
                Serial.println(ds_contatori.termoCount);  // Print with 10 decimal places
            }
        } else if (rawBufferLen == 4) {
            LkArraylize<packet_sanitaria_termo> converter;
            packet_sanitaria_termo ds_sanitaria_termo = converter.deArraylize(rawBuffer);
            if (ds_sanitaria_termo.sender == 100){
                Serial.println();
                Serial.print("Sanitaria Temp: ");
                Serial.println(ds_sanitaria_termo.temperaturaCaldaC);
                Serial.print("Sanitaria Stato: ");
                Serial.println(ds_sanitaria_termo.stato);  // Print with 10 decimal places
            } else if (ds_sanitaria_termo.sender == 101){
                Serial.println();
                Serial.print("Termo Temp: ");
                Serial.println(ds_sanitaria_termo.temperaturaCaldaC);
                Serial.print("Termo Stato: ");
                Serial.println(ds_sanitaria_termo.stato);  // Print with 10 decimal places
            }           
        } else if (rawBufferLen == 3) {
            LkArraylize<packet_ambiente> converter;
            packet_ambiente ds_ambiente = converter.deArraylize(rawBuffer);
            if (ds_ambiente.sender == 102){
                Serial.println();
                Serial.print("Ambiente Temp: ");
                Serial.println(ds_ambiente.temperaturaC);
            }                    
        }
    }
}

void getStatus(byte stato){
  if (stato == T_COSTANTE){
    Serial.println("=");
  } else if (stato == T_SCENDE){
    Serial.println("<");
  } else if (stato == T_SALE){
    Serial.println(">");
  } else {
    Serial.println("?");    
  }
}
