#include <LkRadioStructure.h>

const int LED_PIN = 13;
const int TRANSMIT_PIN = 12;

struct __attribute__((packed)) StructureA {
    byte sender; // 1 byte
    uint16_t value1; // 2 bytes
    float value2; // 4 bytes
    double value3; // 4 bytes
    char text[11]; // 11 bytes
};

LkRadioStructure<StructureA> radio;

void setup() {
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(115200);
    while (!Serial) {
        ; // aspetta la connessione del porto seriale
    }
    Serial.println("setup");

    radio.globalSetup(2000, TRANSMIT_PIN, -1);  // Solo trasmissione
}

void loop() {
    StructureA dataToSend = prepareData();

    radio.sendStructure(dataToSend);

    Serial.println();

    Serial.println("Transmitted data:");
    Serial.print("sender: ");
    Serial.println(dataToSend.sender);
    Serial.print("value1: ");
    Serial.println(dataToSend.value1);
    Serial.print("value2: ");
    Serial.println(dataToSend.value2, 10);  // Print with 10 decimal places
    Serial.print("value3: ");
    Serial.println(dataToSend.value3, 10);  // Print with 10 decimal places
    Serial.print("text: ");
    Serial.println(dataToSend.text);

    delay(2500);  // Wait for 2.5 seconds before sending again
}

StructureA prepareData() {
    StructureA data;
    data.sender = 1;
    data.value1 = 12345;
    data.value2 = 12345.6789f;
    data.value3 = 2.718;
    strncpy(data.text, "ciao123456", sizeof(data.text) - 1);  // Safe copy
    data.text[sizeof(data.text) - 1] = '\0';  // Ensure null-termination
    return data;
}
