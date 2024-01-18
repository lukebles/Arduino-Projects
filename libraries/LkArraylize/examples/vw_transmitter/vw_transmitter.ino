#include <VirtualWire.h>
#include <LkArraylize.h>
 
const int LED_PIN = 13;
const int TRANSMIT_PIN = 12;

struct TransmitData {
    long sender;
    long recipient;
    long counter1;
    long counter2;
    char text[11];
};

void setup() {
  vw_set_tx_pin(TRANSMIT_PIN);
  vw_setup(2000);  // Bits per sec
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial) {
    ; // aspetta la connessione del porto seriale
  }
  Serial.println("setup");
}

void loop() {
    TransmitData dataToSend = prepareData();
    transmitData(dataToSend);
    delay(5000);  // Wait for 5 seconds before sending again
}

TransmitData prepareData() {
    TransmitData data;
    data.sender = -2147483648;
    data.recipient = -6789123;
    data.counter1 = 1234567;
    data.counter2 = 2147483647;
    strncpy(data.text, "ciao123456", sizeof(data.text) - 1);  // Safe copy
    data.text[sizeof(data.text) - 1] = '\0';  // Ensure null-termination
    return data;
}

void transmitData(const TransmitData &data) {
    uint8_t buffer[sizeof(TransmitData)];
    LkArraylize<TransmitData> converter;
    converter.arraylize(buffer, data);

    Serial.println(sizeof(buffer));

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
