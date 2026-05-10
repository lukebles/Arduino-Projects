#include <WiFi.h>

String encTypeToStr(uint8_t enc) {
  switch ((wifi_auth_mode_t)enc) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK: return "WAPI";
    default: return "UNKNOWN";
  }
}

void printMac(const uint8_t* mac) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 16) Serial.print("0");
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
}

void scanWiFi() {
  Serial.println();
  Serial.println("Scansione WiFi in corso...");

  int n = WiFi.scanNetworks(false, true);

  if (n <= 0) {
    Serial.println("Nessuna rete trovata.");
    return;
  }

  Serial.println();
  Serial.println("Idx | RSSI | CH | Cifratura | BSSID/MAC           | SSID");
  Serial.println("----+------+----+-----------+---------------------+--------------------------");

  for (int i = 0; i < n; i++) {
    String ssid;
    uint8_t enc;         // <<< qui la correzione
    int32_t rssi;
    uint8_t* bssid;
    int32_t channel;

    if (WiFi.getNetworkInfo(i, ssid, enc, rssi, bssid, channel)) {
      char line[40];
      snprintf(line, sizeof(line), "%3d | %4ld | %2ld | ", i + 1, (long)rssi, (long)channel);
      Serial.print(line);

      String encStr = encTypeToStr(enc);
      Serial.print(encStr);
      for (int k = encStr.length(); k < 9; k++) Serial.print(" ");
      Serial.print(" | ");

      printMac(bssid);
      Serial.print(" | ");
      Serial.println(ssid);
    }
  }

  WiFi.scanDelete();
}

void setup() {
  Serial.begin(9600);
  delay(1000);

  Serial.println();
  Serial.println("ESP32 WiFi Scanner - AP visibili");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(200);

  scanWiFi();
}

void loop() {
  delay(10000);
  scanWiFi();
}