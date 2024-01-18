#include <LkRadioStructure.h>

const int TRANSMIT_PIN = 12;
const int RECEIVE_PIN = 11;
const int RADIO_SPEED = 2000;
const int LED_PIN = 13;

struct MyStruct {
  uint8_t mittente;
  unsigned long timestamp;
};

LkRadioStructure<MyStruct> myStructRadio;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  LkRadioStructure<MyStruct>::globalSetup(RADIO_SPEED, TRANSMIT_PIN, RECEIVE_PIN);

  Serial.begin(115200);
  while (!Serial) {
    ; // aspetta la connessione del porto seriale
  }
  Serial.println("setup");
}

void loop() {
  if (myStructRadio.have_structure()) {
    digitalWrite(LED_PIN, HIGH);
    processReceivedStructure();
    digitalWrite(LED_PIN, LOW);
  }
}

void processReceivedStructure() {
  MyStruct receivedData = myStructRadio.getStructure();
  printMyStruct(receivedData);
}

void printMyStruct(const MyStruct &data) {
  Serial.println("Ricevuti: ");
  Serial.print("Mittente: "); Serial.println(data.mittente);
  Serial.print("Timestamp: "); Serial.println(data.timestamp);
}
