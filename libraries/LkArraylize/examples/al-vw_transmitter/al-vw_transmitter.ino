#include <VirtualWire.h>
#include <LkArraylize.h>

const int LED_PIN = 13;
const int TRANSMIT_PIN = 12;

struct __attribute__((packed)) TransmitData {
    byte sender;
    uint16_t value1;
    float value2;
    double value3;
    char text[11];
};

TransmitData dataToSend;

void printBufferHex(uint8_t* buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (buffer[i] < 16) {
            Serial.print("0");
        }
        Serial.print(buffer[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void setup() {
    vw_set_tx_pin(TRANSMIT_PIN);
    vw_setup(2000);  // Bits per second
    pinMode(LED_PIN, OUTPUT);

    Serial.begin(115200);
    while (!Serial) {
        ; // aspetta la connessione seriale
    }
    Serial.println("setup");
}

void loop() {
    dataToSend = prepareData();
    Serial.println();
    transmitData(dataToSend);
    delay(5000);  // Wait for 5 seconds before sending again
}

TransmitData prepareData() {
    TransmitData data;
    data.sender = 1;
    data.value1 = 12345;
    data.value2 = 3.141592654f;
    data.value3 = 2.718;
    strncpy(data.text, "ciao123456", sizeof(data.text) - 1);  // Safe copy
    data.text[sizeof(data.text) - 1] = '\0';  // Ensure null-termination
    return data;
}

void transmitData(const TransmitData &data) {
    uint8_t buffer[sizeof(TransmitData)];
    LkArraylize<TransmitData> converter;
    converter.arraylize(buffer, data);

    Serial.println("Transmitted data:");

    Serial.println("Serialized data (hex):");
    printBufferHex(buffer, sizeof(buffer));
    
    Serial.print("sender: ");
    Serial.println(dataToSend.sender);
    Serial.print("value1: ");
    Serial.println(dataToSend.value1);
    Serial.print("value2: ");
    Serial.println(dataToSend.value2, 10);
    Serial.print("value3: ");
    Serial.println(dataToSend.value3, 10);
    Serial.print("text: ");
    Serial.println(dataToSend.text);

    digitalWrite(LED_PIN, HIGH);

    long startTime = millis();
  
    vw_send(buffer, sizeof(buffer));
    vw_wait_tx();

    long endTime = millis();
    double transmissionTimeSec = (endTime - startTime) / 1000.0;

    Serial.print("Bytes sent: ");
    Serial.println(sizeof(buffer));
    Serial.print("Transmission time (s): ");
    Serial.println(transmissionTimeSec, 3);

    double speedBytesSec = sizeof(buffer) / transmissionTimeSec;
    Serial.print("Speed bytes/sec: ");
    Serial.println(speedBytesSec, 0);

    digitalWrite(LED_PIN, LOW);
}
