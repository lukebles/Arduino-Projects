#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "config.h"


// Inizializzazione client e bot
WiFiClientSecure client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

unsigned long lastCheckTime = 0;
const unsigned long botRequestDelay = 1000; // Tempo tra richieste in millisecondi

// GPIO per il LED
const int ledPin = 2;

void setup() {
  // Configurazione GPIO
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH); // LED spento inizialmente
  
  // Avvio della seriale
  Serial.begin(115200);
  delay(1000);
  Serial.println("Connessione al WiFi...");

  // Connessione al WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastCheckTime > 1000) {
      digitalWrite(ledPin, LOW);
      delay(30);
      digitalWrite(ledPin, HIGH);
      lastCheckTime = millis();
    }
  }
  Serial.println("\nConnesso al WiFi");
  Serial.print("Indirizzo IP: ");
  Serial.println(WiFi.localIP());

  // Configurazione client per Telegram
  client.setInsecure(); // Disabilita verifica del certificato SSL

// Invia messaggio di benvenuto su Telegram
  if (bot.sendMessage(TELEGRAM_CHAT_ID, "ESP01 connesso a Internet!", "")) {
    Serial.println("Messaggio inviato con successo!");
  } else {
    Serial.println("Errore nell'invio del messaggio!");
  }
}

void loop() {
  // Controlla nuovi messaggi ogni secondo
  if (millis() - lastCheckTime > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastCheckTime = millis();
  }
}

// Gestione nuovi messaggi
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    //String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    Serial.println(TELEGRAM_CHAT_ID);
    Serial.println(text);
    Serial.println("--------");
    if (text == "/accendi") {
      digitalWrite(ledPin, LOW);
      bot.sendMessage(TELEGRAM_CHAT_ID, "LED acceso!", "");
    } else if (text == "/spegni") {
      digitalWrite(ledPin, HIGH);
      bot.sendMessage(TELEGRAM_CHAT_ID, "LED spento!", "");
    } else {
      bot.sendMessage(TELEGRAM_CHAT_ID, "Comando non riconosciuto. Usa /accendi o /spegni.", "");
    }
  }
}


