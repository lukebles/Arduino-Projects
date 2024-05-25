#include <VirtualWire.h>
#include <LkArraylize.h>

const int RECEIVE_PIN = 11;
const int LED_PIN = 13;

struct __attribute__((packed)) TransmitData {
    byte sender;
    uint16_t value1;
    float value2;
    double value3;
    char text[11];
};

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
    vw_set_rx_pin(RECEIVE_PIN);
    vw_setup(2000);  // Bits per second
    vw_rx_start();   // Start the receiver PLL running
    pinMode(LED_PIN, OUTPUT);

    Serial.begin(115200);
    while (!Serial) {
        ; // aspetta la connessione seriale
    }
    Serial.println("setup");
}

void loop() {
    uint8_t buffer[VW_MAX_MESSAGE_LEN];
    uint8_t buflen = VW_MAX_MESSAGE_LEN;

    if (vw_get_message(buffer, &buflen)) { // Non-blocking
        digitalWrite(LED_PIN, HIGH); // Flash a light to show received good message

        LkArraylize<TransmitData> converter;
        TransmitData receivedData = converter.deArraylize(buffer);

        Serial.println();

        // Print the received values
        Serial.println("Received data:");

        Serial.println("Received data (hex):");
        printBufferHex(buffer, buflen);

        Serial.print("sender: ");
        Serial.println(receivedData.sender);
        Serial.print("value1: ");
        Serial.println(receivedData.value1);
        Serial.print("value2: ");
        Serial.println(receivedData.value2, 10);
        Serial.print("value3: ");
        Serial.println(receivedData.value3, 10);
        Serial.print("text: ");
        Serial.println(receivedData.text);

        digitalWrite(LED_PIN, LOW);
    }
}
