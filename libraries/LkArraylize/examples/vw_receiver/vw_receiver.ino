#include <VirtualWire.h>
#include <LkArraylize.h>
#include <LkSetupSerial.h>

LkSetupSerial mySerial; // altrimenti viene impostato a 115200

const int LED_PIN = 13;
const int RECEIVE_PIN = 11;

struct DataPacket {
    long sender;
    long recipient;
    long counter1;
    long counter2;
    char text[11];
};

void setup() {
  vw_set_rx_pin(RECEIVE_PIN);
  vw_setup(2000);  // Bits per sec
  vw_rx_start();   // Start the receiver PLL running
  pinMode(LED_PIN, OUTPUT);

  mySerial.begin(); 
}

void loop() {
    uint8_t buf[VW_MAX_MESSAGE_LEN];
    uint8_t buflen = VW_MAX_MESSAGE_LEN;

    if (vw_have_message() && vw_get_message(buf, &buflen)) {
        digitalWrite(LED_PIN, HIGH); 
        processReceivedMessage(buf, buflen);
        digitalWrite(LED_PIN, LOW);
    }
}

void processReceivedMessage(uint8_t *buf, uint8_t buflen) {
    if (buflen == sizeof(DataPacket)) {
        LkArraylize<DataPacket> dataPacketConverter;
        DataPacket receivedData = dataPacketConverter.deArraylize(buf);

        printDataPacket(receivedData);
    }
}

void printDataPacket(const DataPacket &data) {
    Serial.println("Ricevuti:");
    Serial.print("Sender: "); Serial.println(data.sender);
    Serial.print("Recipient: "); Serial.println(data.recipient);
    Serial.print("Counter1: "); Serial.println(data.counter1);
    Serial.print("Counter2: "); Serial.println(data.counter2);
    Serial.print("Text: "); Serial.println(data.text);
}
