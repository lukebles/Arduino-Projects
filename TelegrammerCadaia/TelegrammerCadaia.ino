#include <EEPROM.h> // Libreria EEPROM
#include <math.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "config.h" // password & C

#define DEBUG 0 // 0 off 1 on

#if DEBUG
#define prt(x) Serial.print(x)
#define prtn(x) Serial.println(x)
#else
#define prt(x)
#define prtn(x)
#endif

#define LED_PIN 2
#define REMOTO_PIN 0
#define EEPROM_STATE_ADDR 0 // Indirizzo EEPROM per salvare lo stato

const unsigned long botRequestDelay = 1000; // Tempo tra richieste in millisecondi

// Inizializzazione client e bot
WiFiClientSecure client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

bool remotoStato = false; // Variabile per lo stato di remoto

void salvaStato(bool stato) {
  EEPROM.begin(512); // Inizializza EEPROM
  EEPROM.write(EEPROM_STATE_ADDR, stato); // Salva stato (1=acceso, 0=spento)
  EEPROM.commit(); // Scrive su EEPROM
  EEPROM.end(); // Termina sessione EEPROM
}

bool caricaStato() {
  EEPROM.begin(512); // Inizializza EEPROM
  bool stato = EEPROM.read(EEPROM_STATE_ADDR); // Legge stato
  EEPROM.end(); // Termina sessione EEPROM
  return stato;
}

void setup() {
  Serial.begin(115200);
  prtn("Avvio ESP01");
  pinMode(LED_PIN, OUTPUT);
  pinMode(REMOTO_PIN, OUTPUT);

  delay(1000);

  // Connessione al WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    prt(".");
  }

  client.setInsecure();

  if (bot.sendMessage(TELEGRAM_CHAT_ID, "Caldaia connessa. Usa /accendi /spegni /stato", "")) {
    prtn("Messaggio inviato con successo!");
  } else {
    prtn("Errore nell'invio del messaggio!");
  }


  // Carica stato salvato da EEPROM
  remotoStato = caricaStato();
  digitalWrite(LED_PIN, remotoStato ? LOW : HIGH);
  digitalWrite(REMOTO_PIN, remotoStato ? HIGH : LOW);

}

void loop() {
  static unsigned long lastCheckTime = 0;
  static bool nessuProblemaWiFi=true;

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    prt(".");

    nessuProblemaWiFi = false;
  } else {
    // ricarica lo stato del led da eprom
    if (!nessuProblemaWiFi){
      nessuProblemaWiFi = true;
      // Carica stato salvato da EEPROM
      remotoStato = caricaStato();
      digitalWrite(LED_PIN, remotoStato ? LOW : HIGH);
      //digitalWrite(REMOTO_PIN, remotoStato ? HIGH : LOW);

    }
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
      digitalWrite(LED_PIN, LOW);
      digitalWrite(REMOTO_PIN, HIGH);
      salvaStato(remotoStato);
      bot.sendMessage(TELEGRAM_CHAT_ID, "remoto acceso!", "");
    } else if (text == "/spegni") {
      remotoStato = false;
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(REMOTO_PIN, LOW);
      salvaStato(remotoStato);
      bot.sendMessage(TELEGRAM_CHAT_ID, "remoto spento!", "");
    } else if (text == "/stato") {
      String statoMessaggio = remotoStato ? "remoto acceso." : "remoto spento.";
      bot.sendMessage(TELEGRAM_CHAT_ID, "Stato attuale: " + statoMessaggio, "");
    } else {
      bot.sendMessage(TELEGRAM_CHAT_ID, "Caldaia: comando non riconosciuto. Usa /accendi, /spegni o /stato.", "");
    }
  }
}