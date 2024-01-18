#include <LkRadioStructure.h>

const int LED_PIN = 13;
const int TRANSMIT_PIN = 12;
const int RECEIVE_PIN = 11; // unused
const int RADIO_SPEED = 2000;

struct MyStruct {
  uint8_t sender;
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
  MyStruct dataToSend = prepareMyStruct();
  sendMyStruct(dataToSend);
  delay(5000);  // Wait for 5 seconds before sending again
}

MyStruct prepareMyStruct() {
  MyStruct data;
  data.sender = 123;
  data.timestamp = 100000000L;
  return data;
}

void sendMyStruct(const MyStruct &data) {
  digitalWrite(LED_PIN, HIGH);

  long startTime = millis();

  myStructRadio.sendStructure(data);

  long endTime = millis();
  double transmissionTimeSec = (endTime - startTime) / 1000.0;

  Serial.print("Transmission time (s): ");
  Serial.println(transmissionTimeSec, 3);

  digitalWrite(LED_PIN, LOW);
}
