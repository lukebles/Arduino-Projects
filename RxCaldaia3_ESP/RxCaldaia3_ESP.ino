#include <LkRadioStructureEx.h>

const int LED_PIN = 13;
const int RECEIVE_PIN = 0;

#define SANITARIA_calda 0
#define SANITARIA_fredda 4
#define TERMO_calda 2
#define TERMO_fredda 3
#define AMBIENTE 1

#define T_SALE 2
#define T_SCENDE 0
#define T_COSTANTE 1
#define T_STRANA 3 // se temperatura calda < temp fredda

float previousTemp[5];
byte stato[5];

// float temperatura = 25.37;  // Temperatura da inviare
// int16_t temperaturaCodificata = (int16_t)(temperatura * 100);

struct __attribute__((packed)) packet_caldaia {
    byte sender;             // 1 byte
    uint16_t sanitariaCount; // 2 bytes
    uint16_t termoCount;     // 2 bytes
    byte statoSanitaria;     // 1 byte 0=discesa 1=costante 2=salita
    byte statoTermo;         // 1 byte 0=discesa 1=costante 2=salita
    int16_t ambienteC;       // 2 bytes
    int16_t sanitCaldaC;     // 2 bytes
    int16_t termoCaldaC;     // 2 bytes
};

LkRadioStructureEx<packet_caldaia> radio;

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
    radio.globalSetup(2000, -1, RECEIVE_PIN);  // Solo ricezione
}

void loop() {
    if (radio.haveRawMessage()) {
        uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
        uint8_t rawBufferLen;
        radio.getRawBuffer(rawBuffer, rawBufferLen);

        // Deserializza il messaggio nel tipo corretto di struttura
        LkArraylize<packet_caldaia> converter;
        packet_caldaia receivedData = converter.deArraylize(rawBuffer);
        
        if (rawBufferLen == 13) {
            if (receivedData.sender == 99){
                Serial.println();
                Serial.print("Sanitaria count: ");
                Serial.println(receivedData.sanitariaCount);
                Serial.print("Termo count:     ");
                Serial.println(receivedData.termoCount);  // Print with 10 decimal places
                Serial.print("stato sanitaria: ");
                getStatus(receivedData.statoSanitaria);  // Print with 10 decimal places
                Serial.print("stato Termo:     ");
                getStatus(receivedData.statoTermo);
                Serial.print("Sanitaria °C:    ");
                Serial.println(receivedData.sanitCaldaC / 100.0);
                Serial.print("Termo     °C:    ");
                Serial.println(receivedData.termoCaldaC / 100.0);
                Serial.print("Ambiente  °C:    ");
                Serial.println(receivedData.ambienteC / 100.0);
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
