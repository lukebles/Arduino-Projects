#include <ESP8266WiFi.h>

// ... altre definizioni ...

void setup() {
  Serial.begin(115200);
  //WiFi.begin(ssid, password);

  Serial.println("Scanning WiFi networks...");
  int n = WiFi.scanNetworks();
  Serial.println("Scan done.");
  if (n == 0) {
    Serial.println("No networks found.");
  } else {
    for (int i = 0; i < n; ++i) {
      // Stampa i dettagli di ogni rete trovata (opzionale)
      Serial.print("Network Name: ");
      Serial.println(WiFi.SSID(i));
      Serial.print("Security: ");
      printEncryptionType(WiFi.encryptionType(i));
      Serial.println();
      delay(10);
    }
  }
}

void loop() {
  // ... implementazione del loop ...
}

void printEncryptionType(int thisType) {
  // Switch case per determinare il tipo di sicurezza
  switch (thisType) {
    case ENC_TYPE_WEP:
      Serial.println("WEP");
      break;
    case ENC_TYPE_TKIP:
      Serial.println("WPA/PSK");
      break;
    case ENC_TYPE_CCMP:
      Serial.println("WPA2/PSK");
      break;
    case ENC_TYPE_NONE:
      Serial.println("None");
      break;
    case ENC_TYPE_AUTO:
      Serial.println("Auto");
      break;
  }
}

void checkConnectedNetworkSecurity() {
  if (WiFi.status() == WL_CONNECTED) {
    for (int i = 0; i < WiFi.scanNetworks(); ++i) {
      if (WiFi.SSID() == WiFi.SSID(i)) {
        Serial.print("Connected to: ");
        Serial.println(WiFi.SSID());
        Serial.print("Security: ");
        printEncryptionType(WiFi.encryptionType(i));
        break;
      }
    }
  }
}
