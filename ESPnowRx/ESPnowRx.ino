#include <ESP8266WiFi.h>
#include <espnow.h>

void onReceive(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len > 0) {
    uint8_t command = incomingData[0]; // Legge il primo byte del messaggio
    if (command == 1) {
      digitalWrite(LED_BUILTIN, LOW); // Accende il LED (il LED è attivo basso)
      Serial.println("Ricevuto: Acceso");
    } else {
      digitalWrite(LED_BUILTIN, HIGH); // Spegne il LED
      Serial.println("Ricevuto: Spento");
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Configura il LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Spegne inizialmente il LED

  // Modalità STATION
  WiFi.mode(WIFI_STA);
  
  // Inizializzazione ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Errore durante l'inizializzazione di ESP-NOW");
    return;
  }
  
  // Registra callback per ricezione
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onReceive);
}

void loop() {
  // Nessuna operazione nel loop principale
}