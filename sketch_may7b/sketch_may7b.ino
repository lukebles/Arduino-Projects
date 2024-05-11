#include <ESP8266WiFi.h>
#include <espnow.h>

// uint8_t remoteMac[] = {0xe0, 0x98, 0x06, 0x80, 0xe3, 0x3f};  // Sostituisci con l'indirizzo MAC di B
uint8_t remoteMac[] = {0x8c, 0xaa, 0xb5, 0x0b, 0x54, 0x98};  // Sostituisci con l'indirizzo MAC di B
unsigned long lastTime = 0;
unsigned long interval = 1000;
bool initiator = false;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != 0) {
    Serial.println("Errore nell'inizializzazione di ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_add_peer(remoteMac, ESP_NOW_ROLE_COMBO, 1, NULL, 0);

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // Inizializza randomicamente se iniziare a inviare
  randomSeed(micros());
  if (random(0, 2)) {
    initiator = true;
    sendRandomNumber();
  }

  Serial.println("OK setup");
}

void loop() {
  if (!initiator && millis() - lastTime > interval) {
    sendRandomNumber();
  }
}

void sendRandomNumber() {
  int randomNumber = random(0, 100);
  esp_now_send(remoteMac, (uint8_t *)&randomNumber, sizeof(randomNumber));
  lastTime = millis();
}

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Stato invio: ");
  Serial.println(sendStatus == 0 ? "Success" : "Fail");
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  int number = *(int *)incomingData;
  Serial.print("Numero ricevuto: ");
  Serial.println(number);
  delay(1000);
  sendRandomNumber();
}
