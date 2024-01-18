#include <LkRadioStructure.h>

const int LED_PIN = 13;
const int TRANSMIT_PIN = 12;
const int RECEIVE_PIN = 11;
const int RADIO_SPEED = 2000;

struct MyStruct {
  uint8_t sender; // 1 byte
  uint8_t function; // 1 byte
  unsigned long timestamp; // Arduino Uno: 4 bytes 
};

struct MyStructB {
  uint8_t sender; // 1 byte
  uint8_t function; // 1 byte
  int integer1; // Arduino Uno: 2 bytes 
  int integer2; // Arduino Uno: 2 bytes
};

LkRadioStructure<MyStruct> myStructRadio;
LkRadioStructure<MyStructB> myStructBRadio;

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
  sendMyStruct();
  delay(1000);

  sendMyStructB();
  delay(1000);
}

void sendMyStruct() {
  MyStruct data;
  data.sender = 123;
  data.function = 1;
  data.timestamp = 100000000L;

  digitalWrite(LED_PIN, HIGH);
  myStructRadio.sendStructure(data);
  digitalWrite(LED_PIN, LOW);
}

void sendMyStructB() {
  MyStructB data;
  data.sender = 99;
  data.function = 2;
  data.integer1 = -1000;
  data.integer2 = 32109;

  digitalWrite(LED_PIN, HIGH);
  myStructBRadio.sendStructure(data);
  digitalWrite(LED_PIN, LOW);
}
