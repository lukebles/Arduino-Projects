#include <Preferences.h>
#include <math.h>
#include <WiFi.h> // Libreria per ESP32
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "config.h" // Contiene TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID, WIFI_SSID, WIFI_PASSWORD

// const char* telegram_cert = \
// "-----BEGIN CERTIFICATE-----\n" \ 
// "MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \ 
// "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \ 
// "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \ 
// "QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \ 
// "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \ 
// "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \ 
// "9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n" \ 
// "CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n" \ 
// "nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n" \ 
// "43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n" \ 
// "T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n" \ 
// "gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n" \ 
// "BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n" \ 
// "TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n" \ 
// "DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n" \ 
// "hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n" \ 
// "06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n" \ 
// "PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n" \ 
// "YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n" \ 
// "CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n" \ 
// "-----END CERTIFICATE-----\n";

#define DEBUG 0 // 0 off, 1 on

#if DEBUG
#define prt(x) Serial.print(x)
#define prtn(x) Serial.println(x)
#else
#define prt(x)
#define prtn(x)
#endif

#define LED_PIN 2         // GPIO2 (di solito LED integrato su alcuni ESP32)
#define REMOTO_PIN 16      // GPIO16
#define EEPROM_STATE_ADDR 0 // Indirizzo EEPROM per lo stato

Preferences prefs;

const unsigned long botRequestDelay = 1000; // Delay tra richieste Telegram

WiFiClientSecure client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

bool remotoStato = false; // Stato attuale del relè/remoto

void salvaStato(bool stato) {
  prefs.begin("caldaia", false);       // "caldaia" è il namespace
  prefs.putBool("remoto", stato);      // salva bool con chiave "remoto"
  prefs.end();
}


bool caricaStato() {
  prefs.begin("caldaia", true);       // true = sola lettura
  bool stato = prefs.getBool("remoto", false); // default = false
  prefs.end();
  return stato;
}


void setup() {
  Serial.begin(115200);
  prtn("Avvio ESP32");

  pinMode(LED_PIN, OUTPUT);
  pinMode(REMOTO_PIN, OUTPUT);

  delay(1000);

  // Connessione Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoReconnect(true);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    prt(".");
  }

  digitalWrite(LED_PIN, LOW);

  prtn("\nWiFi connesso: " + WiFi.localIP().toString());

  //client.setCACert(telegram_cert);

  client.setInsecure(); // Ignora certificati SSL

  if (bot.sendMessage(TELEGRAM_CHAT_ID, "Caldaia connessa. Usa /accendi /spegni /stato", "")) {
    prtn("Messaggio Telegram inviato!");
  } else {
    prtn("Errore nell'invio Telegram");
  }

  // Carica stato
  remotoStato = caricaStato();
  digitalWrite(LED_PIN, remotoStato ? HIGH : LOW);
  digitalWrite(REMOTO_PIN, remotoStato ? HIGH : LOW);
}

void loop() {
  static unsigned long lastCheckTime = 0;
  static bool nessunProblemaWiFi = true;

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    prt(".");
    digitalWrite(LED_PIN, LOW);
    nessunProblemaWiFi = false;
  } else if (!nessunProblemaWiFi) {
    // Reconnessione riuscita
    nessunProblemaWiFi = true;
    remotoStato = caricaStato();
    digitalWrite(LED_PIN, remotoStato ? HIGH : LOW);
    digitalWrite(REMOTO_PIN, remotoStato ? HIGH : LOW);
  }

  if (millis() - lastCheckTime > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastCheckTime = millis();
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;

    if (text == "/accendi") {
      remotoStato = true;
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(REMOTO_PIN, HIGH);
      salvaStato(remotoStato);
      bot.sendMessage(TELEGRAM_CHAT_ID, "Remoto acceso!", "");
    } else if (text == "/spegni") {
      remotoStato = false;
      digitalWrite(LED_PIN, LOW);
      digitalWrite(REMOTO_PIN, LOW);
      salvaStato(remotoStato);
      bot.sendMessage(TELEGRAM_CHAT_ID, "Remoto spento!", "");
    } else if (text == "/stato") {
      String msg = remotoStato ? "Remoto acceso." : "Remoto spento.";
      bot.sendMessage(TELEGRAM_CHAT_ID, "Stato attuale: " + msg, "");
    } else {
      bot.sendMessage(TELEGRAM_CHAT_ID, "Comando sconosciuto. Usa /accendi /spegni /stato.", "");
    }
  }
}
