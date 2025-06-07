#include <ESP8266WiFi.h>
#include "ThingSpeak.h"
#include "config.h"


WiFiClient client;

#define LED_PIN 2
#define DEBUG 1 // 0 off 1 on

#if DEBUG
#define prt(x) Serial.print(x)
#define prtn(x) Serial.println(x)
#else
#define prt(x)
#define prtn(x)
#endif


void setup() {

  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  digitalWrite(LED_PIN, HIGH);
  prtn();
  prtn("Avvio");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Attiva la riconnessione automatica
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    prt(".");
  }

    ThingSpeak.begin(client);
}

void loop() {


  if (WiFi.status() != WL_CONNECTED) {
    //prtn("Connessione persa, tentativo di riconnessione...");
    WiFi.reconnect();
    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    prt(".");
  }


    float temperature = 25.5; // Sostituisci con il tuo sensore
    ThingSpeak.setField(1, temperature);
    ThingSpeak.setField(2, temperature);
    ThingSpeak.setField(3, temperature);
    ThingSpeak.setField(4, temperature);
    ThingSpeak.setField(5, temperature);
    ThingSpeak.setField(6, temperature);
    ThingSpeak.setField(7, temperature);
    ThingSpeak.setField(8, temperature);
    ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_API_KEY_WRITE);
    delay(15000); // Rispetta il limite di 15 secondi

    // =========== RICEZIONE SERIALE ==========
    byte sender;
    uint8_t data[20]; // Buffer per i dati
    size_t length;
    if (receiveFromMulticatch(sender, data, sizeof(data), length, 1000)) {
      // Pacchetto ricevuto correttamente
      handleReceivedPacket(sender, data, length);
    } else {
      prtn("Timeout o errore nella ricezione del pacchetto.");
    }
    // =======================================

}