#include <ESP8266WiFi.h>

void setup() {
  Serial.begin(115200);
  // Attendi che la porta seriale si apra, utile per le schede che riavviano al collegamento della porta seriale
  while (!Serial) { }
  Serial.println();
  // Mostra la versione del SDK
  Serial.print("SDK Version: ");
  Serial.println(ESP.getSdkVersion());

  // Mostra l'indirizzo MAC del dispositivo
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

}

void loop() {
  // Non è necessario inserire codice nel loop per questo esempio
}
