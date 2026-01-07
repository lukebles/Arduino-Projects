#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(300);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);

  Serial.println("WiFi scan...");
  int n = WiFi.scanNetworks(false, true); // show_hidden=true
  Serial.printf("Reti trovate: %d\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("%02d) %s  RSSI=%d\n", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }
}

void loop() {}
