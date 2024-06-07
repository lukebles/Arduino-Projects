#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include <ESP8266WiFi.h>

const char* ssid = "sid2";
const char* password = "pw12345678";

void setupWiFi() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
}

#endif // WIFI_SETUP_H
