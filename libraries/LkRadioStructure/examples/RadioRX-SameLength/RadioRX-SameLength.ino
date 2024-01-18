#include <LkRadioStructure.h>

const int LED_PIN = 13;
const int TRANSMIT_PIN = 12; // Non utilizzato direttamente
const int RECEIVE_PIN = 11;
const int RADIO_SPEED = 2000;

struct MyStructB {
  uint8_t sender; // 1 byte
  uint8_t function; // 1 byte
  int integer1; // Arduino Uno: 2 bytes 
  int integer2; // Arduino Uno: 2 bytes
};

struct MyStruct {
  uint8_t sender; // 1 byte
  uint8_t function; // 1 byte
  unsigned long timestamp; // Arduino Uno: 4 bytes 
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
  if (myStructRadio.have_structure()) {
    digitalWrite(LED_PIN, HIGH);
    MyStruct receivedData = myStructRadio.getStructure();
    printMyStruct(receivedData);
    digitalWrite(LED_PIN, LOW);
  }

  if (myStructBRadio.have_structure()) {
    digitalWrite(LED_PIN, HIGH);
    MyStructB receivedData = myStructBRadio.getStructure();
    printMyStructB(receivedData);
    digitalWrite(LED_PIN, LOW);
  }
}

void printMyStruct(const MyStruct &data) {
  Serial.println("Received MyStruct:");
  Serial.print("Sender: "); Serial.println(data.sender);
  Serial.print("Function: "); Serial.println(data.function);
  Serial.print("Timestamp: "); Serial.println(data.timestamp);
}

void printMyStructB(const MyStructB &data) {
  Serial.println("Received MyStructB:");
  Serial.print("Sender: "); Serial.println(data.sender);
  Serial.print("Function: "); Serial.println(data.function);
  Serial.print("Integer1: "); Serial.println(data.integer1);
  Serial.print("Integer2: "); Serial.println(data.integer2);
}
