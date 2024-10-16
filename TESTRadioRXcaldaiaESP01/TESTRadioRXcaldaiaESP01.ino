#include <LkRadioStructureEx.h>

const int LED_PIN = 13;
const int RECEIVE_PIN = 0;

#define TEMPER_COSTANTE 1
#define TEMPER_SALITA 2
#define TEMPER_DISCESA 0 

struct __attribute__((packed)) packet_caldaia {
    byte sender; // 1 byte
    uint16_t sanitariaCount; // 2 bytes
    uint16_t termoCount; // 2 bytes
    byte statoSanitaria; // 0=discesa 1=costante 2=salita
    byte statoTermo; // 0=discesa 1=costante 2=salita
};

LkRadioStructureEx<packet_caldaia> radio;

void setup() {
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(115200);
    while (!Serial) {
        ; // aspetta la connessione del porto seriale
    }
    Serial.println("setup");

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

        if (rawBufferLen == 7) {
          if (receivedData.sender == 99){
            Serial.println();
            Serial.print("Sanitaria count: ");
            Serial.println(receivedData.sanitariaCount);
            Serial.print("Termo count: ");
            Serial.println(receivedData.termoCount);  // Print with 10 decimal places
            Serial.print("stato sanitaria: ");
            chkStatus(receivedData.statoSanitaria);  // Print with 10 decimal places
            Serial.print("stato Termo: ");
            chkStatus(receivedData.statoTermo);
          }
        }
    }
}

void chkStatus(byte stato){
  if (stato == TEMPER_COSTANTE){
    Serial.println("=");
  } else if (stato == TEMPER_DISCESA){
    Serial.println("<");
  } else {
    Serial.println(">");
  }
}
