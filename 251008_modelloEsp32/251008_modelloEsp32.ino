#include <Arduino.h>

#if __has_include(<esp_mac.h>)
  #include <esp_mac.h>     // IDF5 (Arduino-ESP32 3.x)
#else
  #include <esp_wifi.h>    // IDF4 (Arduino-ESP32 2.x) esp_read_mac è qui
#endif

static void printMac(const char* label, const uint8_t m[6]) {
  Serial.printf("%s %02X:%02X:%02X:%02X:%02X:%02X\n",
                label, m[0], m[1], m[2], m[3], m[4], m[5]);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.printf("Modello: %s\n", ESP.getChipModel());
  Serial.printf("Core:    %d\n", ESP.getChipCores());
  Serial.printf("Rev:     %d\n", ESP.getChipRevision());

#if defined(CONFIG_IDF_TARGET_ESP32)
  Serial.println("✅ Questo chip supporta Bluetooth Classic (BR/EDR) + BLE.");
#else
  Serial.println("❌ Questo chip NON supporta Bluetooth Classic (solo BLE).");
#endif

  // --- MAC Bluetooth ---
  uint8_t mac_bt[6];
  if (esp_read_mac(mac_bt, ESP_MAC_BT) == ESP_OK) {
    printMac("MAC Bluetooth:", mac_bt);
  } else {
    Serial.println("MAC Bluetooth non disponibile (esp_read_mac fallita).");
  }

  // --- MAC Wi-Fi STA (per confronto) ---
  uint8_t mac_sta[6];
  if (esp_read_mac(mac_sta, ESP_MAC_WIFI_STA) == ESP_OK) {
    printMac("MAC Wi-Fi STA:", mac_sta);
  }
}

void loop() { delay(2000); }
