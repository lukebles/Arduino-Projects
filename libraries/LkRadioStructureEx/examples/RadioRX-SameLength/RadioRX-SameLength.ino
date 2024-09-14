#include <LkRadioStructure.h>

const int LED_PIN = 13;
const int RECEIVE_PIN = 11;

struct __attribute__((packed)) StructureA {
    byte sender; // 1 byte
    uint16_t value1; // 2 bytes
    float value2; // 4 bytes
    double value3; // 4 bytes
    char text[11]; // 11 bytes
};

struct __attribute__((packed)) StructureB {
    byte sender; // 1 byte
    double value10; // 4 bytes
    char data[11]; // 11 bytes
    uint16_t value11; // 2 bytes
    float value12; // 4 bytes
};

const size_t SIZE_A = sizeof(StructureA);
const size_t SIZE_B = sizeof(StructureB);

LkRadioStructure<StructureA> radio;

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

        Serial.println();

        Serial.print("Raw buffer length: ");
        Serial.println(rawBufferLen);
        Serial.print("Raw buffer: ");
        for (uint8_t i = 0; i < rawBufferLen; i++) {
            Serial.print(rawBuffer[i], HEX);
            Serial.print(" ");
        }
        Serial.println();

        byte sender = rawBuffer[rawBufferLen - 1];  // Assuming sender is the last byte
        if (sender == 1) {
            LkArraylize<StructureA> converterA;
            StructureA receivedDataA = converterA.deArraylize(rawBuffer);
            Serial.println("Received data for StructureA:");
            Serial.print("sender: ");
            Serial.println(receivedDataA.sender);
            Serial.print("value1: ");
            Serial.println(receivedDataA.value1);
            Serial.print("value2: ");
            Serial.println(receivedDataA.value2, 10);  // Print with 10 decimal places
            Serial.print("value3: ");
            Serial.println(receivedDataA.value3, 10);  // Print with 10 decimal places
            Serial.print("text: ");
            Serial.println(receivedDataA.text);

            Serial.println("----");
            
        } else if (sender == 2) {
            LkArraylize<StructureB> converterB;
            StructureB receivedDataB = converterB.deArraylize(rawBuffer);
            Serial.println("Received data for StructureB:");
            Serial.print("sender: ");
            Serial.println(receivedDataB.sender);
            Serial.print("value10: ");
            Serial.println(receivedDataB.value10, 10);  // Print with 10 decimal places
            Serial.print("data: ");
            Serial.println(receivedDataB.data);
            Serial.print("value11: ");
            Serial.println(receivedDataB.value11);
            Serial.print("value12: ");
            Serial.println(receivedDataB.value12, 10);  // Print with 10 decimal places
        }
    }
}
