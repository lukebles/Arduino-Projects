#include <LkRadioStructureEx.h>

const int LED_PIN = 13;
const int RECEIVE_PIN = 1;

struct __attribute__((packed)) StructureA {
    byte sender; // 1 byte
    uint16_t value1; // 2 bytes
    uint16_t value2; // 4 bytes
};

LkRadioStructureEx<StructureA> radio;

void setup() {
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(115200);
    delay(1500);
    Serial.println();
    Serial.println("setup");

    radio.globalSetup(2000, -1, RECEIVE_PIN);  // Solo ricezione
}

void loop() {
    if (radio.haveRawMessage()) {
        uint8_t rawBuffer[VW_MAX_MESSAGE_LEN];
        uint8_t rawBufferLen;
        radio.getRawBuffer(rawBuffer, rawBufferLen);

        Serial.print("Raw buffer length: ");
        Serial.println(rawBufferLen);
        Serial.print("Raw buffer: ");
        for (uint8_t i = 0; i < rawBufferLen; i++) {
            Serial.print(rawBuffer[i], HEX);
            Serial.print(" ");
        }
        Serial.println();

        // Deserializza il messaggio nel tipo corretto di struttura
        LkArraylize<StructureA> converter;
        StructureA receivedData = converter.deArraylize(rawBuffer);

        Serial.println();

        Serial.println("Received data:");
        Serial.print("sender: ");
        Serial.println(receivedData.sender);
        Serial.print("value1: ");
        Serial.println(receivedData.value1);
        Serial.print("value2: ");
        Serial.println(receivedData.value2);  // Print with 10 decimal places
    }
}
