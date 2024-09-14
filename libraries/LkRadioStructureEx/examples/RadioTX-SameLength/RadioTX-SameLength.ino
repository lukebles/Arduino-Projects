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

struct __attribute__((packed)) StructureB {
    byte sender; // 1 byte
    double value10; // 4 bytes
    char data[11]; // 11 bytes
    uint16_t value11; // 2 bytes
    float value12; // 4 bytes
};

LkRadioStructure<StructureA> radioA;
LkRadioStructure<StructureB> radioB;

void setup() {
    pinMode(LED_PIN, OUTPUT);
    Serial.begin(115200);
    while (!Serial) {
        ; // aspetta la connessione del porto seriale
    }
    Serial.println("setup");

    radioA.globalSetup(2000, TRANSMIT_PIN, -1);  // Solo trasmissione
}

void loop() {
    StructureA dataToSendA = prepareDataA();
    StructureB dataToSendB = prepareDataB();

    radioA.sendStructure(dataToSendA);

    Serial.println("Transmitted data for StructureA:");
    Serial.print("sender: ");
    Serial.println(dataToSendA.sender);
    Serial.print("value1: ");
    Serial.println(dataToSendA.value1);
    Serial.print("value2: ");
    Serial.println(dataToSendA.value2, 10);  // Print with 10 decimal places
    Serial.print("value3: ");
    Serial.println(dataToSendA.value3, 10);  // Print with 10 decimal places
    Serial.print("text: ");
    Serial.println(dataToSendA.text);

    delay(2500);  // Wait for 2.5 seconds before sending again

    radioB.sendStructure(dataToSendB);

    Serial.println("----");

    Serial.println("Transmitted data for StructureB:");
    Serial.print("sender: ");
    Serial.println(dataToSendB.sender);
    Serial.print("value10: ");
    Serial.println(dataToSendB.value10, 10);  // Print with 10 decimal places
    Serial.print("data: ");
    Serial.println(dataToSendB.data);
    Serial.print("value11: ");
    Serial.println(dataToSendB.value11);
    Serial.print("value12: ");
    Serial.println(dataToSendB.value12, 10);  // Print with 10 decimal places

    delay(2500);  // Wait for 2.5 seconds before sending again
}

StructureA prepareDataA() {
    StructureA data;
    data.sender = 1;
    data.value1 = 12345;
    data.value2 = 12345.6789f;
    data.value3 = 2.718;
    strncpy(data.text, "ciao123456", sizeof(data.text) - 1);  // Safe copy
    data.text[sizeof(data.text) - 1] = '\0';  // Ensure null-termination
    return data;
}

StructureB prepareDataB() {
    StructureB data;
    data.sender = 2;
    data.value10 = 3.1415;
    strncpy(data.data, "data1234567", sizeof(data.data) - 1);  // Safe copy
    data.data[sizeof(data.data) - 1] = '\0';  // Ensure null-termination
    data.value11 = 54321;
    data.value12 = 9876.5432f;
    return data;
}
