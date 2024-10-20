#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include <ESP8266WiFi.h>

const char* ssid = "gas4"; // sid3 Marco sid2 Luca
const char* password = "12345678";

int findBestChannel() {
int bestChannel = 1;
  int bestScore = 255;

  for (int channel = 1; channel <= 13; channel++) {
    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssid, password, channel);
    delay(500);

    int n = WiFi.scanNetworks();
    int score = 0;
    for (int i = 0; i < n; ++i) {
      if (WiFi.channel(i) == channel) {
        score++;
      }
    }
    if (score < bestScore) {
      bestScore = score;
      bestChannel = channel;
    }
  }

  return bestChannel;
}


void startWiFiAP(int channel) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password, channel);
    Serial.print("WiFi AP started on channel ");
    Serial.println(channel);
}

void scanWiFiChannels() {
   //int numNetworks = WiFi.scanNetworks();
    int bestChannel = findBestChannel();
    startWiFiAP(bestChannel);
}

void setupWiFi() {
    scanWiFiChannels();
}

#endif // WIFI_SETUP_H
