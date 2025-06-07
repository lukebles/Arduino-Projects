#include <ESP8266WiFi.h>
#include <espnow.h>

// MAC address del ricevitore
uint8_t receiverAddress[] = {0xec, 0xfa, 0xbc, 0x37, 0x99, 0xf3}; // Cambia con il MAC del ricevitore

// Variabile per alternare il segnale
bool ledState = false;

void onSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Status: ");
  Serial.println(sendStatus == 0 ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);
  
  // Modalità STATION
  WiFi.mode(WIFI_STA);
  
  // Inizializzazione ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Errore durante l'inizializzazione di ESP-NOW");
    return;
  }
  
  // Registra callback per stato invio
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(onSent);
  
  // Aggiungi il peer (ricevitore)
  esp_now_add_peer(receiverAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
}

void loop() {
  // Alterna il comando
  ledState = !ledState;
  uint8_t dataToSend = ledState ? 1 : 0;

  // Invia il comando
  esp_now_send(receiverAddress, &dataToSend, sizeof(dataToSend));
  Serial.println(ledState ? "Inviato: Acceso" : "Inviato: Spento");
  
  delay(500); // Aspetta mezzo secondo
}