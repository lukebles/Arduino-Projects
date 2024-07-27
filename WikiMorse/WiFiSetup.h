#ifndef WIFISETUP_H
#define WIFISETUP_H

#include <ESP8266WiFi.h>

const char* ssid = "wkm";
const char* password = "12345678";

void setupWiFi() {
  Serial.println("Configurazione WiFi come Access Point...");
  
  // Impostazione del WiFi come Access Point
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Access Point configurato. IP address: ");
  Serial.println(IP);
}

#endif // WIFISETUP_H