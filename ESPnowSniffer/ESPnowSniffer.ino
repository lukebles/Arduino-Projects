#include <ESP8266WiFi.h>
#include <espnow.h>

// Variabili globali
unsigned long lastPacketTime = 0;  // Tempo dell'ultimo pacchetto ricevuto
const unsigned long timeout = 10000; // Timeout di 10 secondi
bool channelFound = false;          // Stato del canale attivo
int currentChannel = 1;             // Canale corrente

// Callback per la ricezione dei pacchetti
void onReceive(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  // Aggiorna il tempo dell'ultimo pacchetto ricevuto
  lastPacketTime = millis();

  // Stampa il MAC del mittente
  Serial.print("Pacchetto ricevuto da: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }

  // Stampa i dati ricevuti
  Serial.print(" | Dati: ");
  for (int i = 0; i < len; i++) {
    Serial.printf("%02X ", incomingData[i]);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);

  // Imposta la modalità WiFi su STATION
  WiFi.mode(WIFI_STA);

  // Inizializza ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Errore nell'inizializzazione di ESP-NOW");
    return;
  }

  // Registra il callback per la ricezione dei pacchetti
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onReceive);

  Serial.println("Sniffer ESP-NOW avviato");
}

void loop() {
  if (!channelFound) {
    // Scansiona i canali WiFi da 1 a 13
    for (int channel = 1; channel <= 13; channel++) {
      wifi_set_channel(channel);
      Serial.print("Monitorando il canale: ");
      Serial.println(channel);

      // Attendi per un breve periodo per intercettare i pacchetti
      delay(500);

      // Se viene ricevuto un pacchetto, fermati su questo canale
      if (millis() - lastPacketTime < 500) {
        channelFound = true;
        currentChannel = channel;
        Serial.print("Segnale trovato sul canale: ");
        Serial.println(channel);
        break;
      }
    }
  } else {
    // Resta sul canale corrente
    Serial.print("Monitorando il canale attivo: ");
    Serial.println(currentChannel);

    // Controlla se il timeout è stato superato
    if (millis() - lastPacketTime > timeout) {
      Serial.println("Nessun pacchetto ricevuto per 10 secondi, riprendo la scansione...");
      channelFound = false; // Riprendi la scansione
    }

    // Attendi un po' prima di controllare di nuovo
    delay(1000);
  }
}