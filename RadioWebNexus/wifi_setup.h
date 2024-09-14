#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include <ESP8266WiFi.h>

const char* ssid = "rwn"; // sid3 Marco sid2 Luca
const char* password = "12345678";

int findBestChannel(int numNetworks) {
    // Array to count the number of networks on each channel
    int channels[13] = {0};

    // Count the number of networks on each channel
    for (int i = 0; i < numNetworks; ++i) {
        int channel = WiFi.channel(i);
        if (channel >= 1 && channel <= 13) {
            channels[channel - 1]++;
        }
    }

    // Find the channel with the least networks
    int bestChannel = 1;
    int minNetworks = channels[0];
    for (int i = 1; i < 13; ++i) {
        if (channels[i] < minNetworks) {
            bestChannel = i + 1;
            minNetworks = channels[i];
        }
    }

    Serial.print("Best channel: ");
    Serial.println(bestChannel);
    return bestChannel;
}


void startWiFiAP(int channel) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password, channel);
    Serial.print("WiFi AP started on channel ");
    Serial.println(channel);
}

void scanWiFiChannels() {
    int numNetworks = WiFi.scanNetworks();
    int bestChannel = findBestChannel(numNetworks);
    startWiFiAP(bestChannel);
}

void setupWiFi() {
    scanWiFiChannels();
}

#endif // WIFI_SETUP_H
