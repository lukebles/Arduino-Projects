#include <math.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "config.h" // password & C

#define DEBUG 0 // 0 off 1 on
// Definizioni costanti
#define MAX_SERIAL_BUFFER 64

// Variabili globali
char serialBuffer[MAX_SERIAL_BUFFER];
int bufferIndex = 0;

#if DEBUG
#define prt(x) Serial.print(x)
#define prtn(x) Serial.println(x)
#else
#define prt(x)
#define prtn(x)
#endif

#define POWER_LIMIT 3990
#define DEFAULT_LIMIT 9999

// Costanti per i sender
#define SENDER_ENERGY 1
#define SENDER_H2O_CALORIE 99
#define SENDER_H2O_SANITARIA 100
#define SENDER_H2O_TERMOSIFONI 101
#define SENDER_H2O_STATO 102
#define SENDER_METEO_TEMP 106
#define SENDER_METEO_UMIDITA 107
#define SENDER_METEO_PRESSIONE 108

const int LED_PIN = 2;

const unsigned long botRequestDelay = 1000; // Tempo tra richieste in millisecondi

typedef struct {
    int temp;
    int humidity;
    int pressure;
    int sanitaria_calda;
    int sanitaria_fredda;
    int termo_calda;
    int termo_fredda;
    int potenza;
} SensorValues;

static SensorValues prev_values = {
                                  DEFAULT_LIMIT, DEFAULT_LIMIT, DEFAULT_LIMIT, 
                                  DEFAULT_LIMIT, DEFAULT_LIMIT, DEFAULT_LIMIT, 
                                  DEFAULT_LIMIT, DEFAULT_LIMIT
                                  };

// Inizializzazione client e bot
WiFiClientSecure client;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, client);

unsigned long lastCheckTime = 0;

// Funzioni per elaborare i pacchetti ricevuti
void processEnergyPacket(uint8_t* data, size_t length) {
  // contatori
  if (length != sizeof(uint16_t) * 2 + sizeof(unsigned long)) {
    return;
  }

  uint16_t diffA = *((uint16_t*)&data[0]);
  uint16_t diffB = *((uint16_t*)&data[2]);
  unsigned long diffTime = *((unsigned long*)&data[4]);

  // Calcolo della potenza istantanea
  float power = (diffA * 3600.0) / (diffTime / 1000.0);
  int int_power = round(power);

  if (int_power > POWER_LIMIT){
    if (prev_values.potenza != int_power){
      prev_values.potenza = int_power;
      // send message
      String fullMessage = "potenza " + String(int_power) + " W";
      if (bot.sendMessage(TELEGRAM_CHAT_ID, fullMessage, "")) {
        // messaggio inviato
      } else {
        // messaggio non inviato
      }
    }
  }
}

void processGasPacket(uint8_t sender, uint8_t* data, size_t length) {
    // Controlla la lunghezza del pacchetto
    if (length != sizeof(uint16_t) * 2) {
        return;
    }

    // Decodifica i dati in modo sicuro
    uint16_t valueB, valueC;
    memcpy(&valueB, &data[0], sizeof(uint16_t));
    memcpy(&valueC, &data[2], sizeof(uint16_t));

    // Funzione helper per inviare aggiornamenti
    auto sendGasUpdate = [](int& prevValue, int newValue, const char* label, const char* unit) {
        int i_new = round(float(newValue) / 100.0);
        
        if (prevValue != i_new) {
            prevValue = i_new;
            char fullMessage[50];
            snprintf(fullMessage, sizeof(fullMessage), "%s %d %s", label, i_new, unit);
            if (!bot.sendMessage(TELEGRAM_CHAT_ID, fullMessage, "")) {
                // Gestione errore di invio
            }
        }
    };

    // Gestione dei pacchetti in base al sender
    switch (sender) {
        case SENDER_H2O_SANITARIA:
            sendGasUpdate(prev_values.sanitaria_fredda, valueC, "h20 fredda", "°C");
            sendGasUpdate(prev_values.sanitaria_calda, valueB, "h20 calda", "°C");
            break;

        case SENDER_H2O_TERMOSIFONI:
            sendGasUpdate(prev_values.termo_fredda, valueC, "termo fredda", "°C");
            sendGasUpdate(prev_values.termo_calda, valueB, "termo calda", "°C");
            break;

        default:
            // Caso non riconosciuto
            break;
    }
}


void processMeteoPacket(uint8_t sender, uint8_t* data, size_t length) {

    digitalWrite(LED_PIN,LOW);
    delay(300);
    digitalWrite(LED_PIN,HIGH);
    if (length != sizeof(float)) {
        return;
    }

    float valueA;
    memcpy(&valueA, data, sizeof(float));
    int int_valueA = round(valueA);

    auto sendMeteoUpdate = [](int& prevValue, int newValue, const char* label, const char* unit) {
        if (prevValue != newValue) {
            prevValue = newValue;
            char fullMessage[50];
            snprintf(fullMessage, sizeof(fullMessage), "meteo %s %d %s", label, newValue, unit);
            if (!bot.sendMessage(TELEGRAM_CHAT_ID, fullMessage, "")) {
                // Gestione errore
            }
        }
    };

    switch (sender) {
        case SENDER_METEO_TEMP:
            sendMeteoUpdate(prev_values.temp, int_valueA, "temp", "°C");
            break;
        case SENDER_METEO_UMIDITA:
            sendMeteoUpdate(prev_values.humidity, int_valueA, "umidita", "%");
            break;
        case SENDER_METEO_PRESSIONE:
            sendMeteoUpdate(prev_values.pressure, int_valueA, "press", "hPa");
            break;
        default:
            break;
    }
}

void processStatusPacket(uint8_t* data, size_t length) {
  if (length != 2) {
    return;
  }

  byte valueF = data[0];
  byte valueG = data[1];
}

void handleReceivedPacket(uint8_t sender, uint8_t* data, size_t length) {


  switch (sender) {
    case 1:
      processEnergyPacket(data, length);
      break;
    case 99:
      processGasPacket(sender, data, length);
      break;
    case 100:
    case 101:
      processGasPacket(sender, data, length);
      break;
    case 102:
      processStatusPacket(data, length);
      break;
    case 106:
    case 107:
    case 108:
      processMeteoPacket(sender, data, length);
      break;
    default:
      prt("Sender non riconosciuto: ");
      prtn(sender);
      break;
  }
}


void setup() {
  Serial.begin(115200);
  prtn("Avvio ESP01");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  digitalWrite(LED_PIN, HIGH);
// Connessione al WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    prt(".");
  }

  // Disabilita la verifica del certificato SSL
  client.setInsecure();

  if (bot.sendMessage(TELEGRAM_CHAT_ID, "ESP01 connesso a Internet", "")) {
    prtn("Messaggio inviato con successo!");
  } else {
    prtn("Errore nell'invio del messaggio!");
  }      
}

bool receiveFromWebNexus(byte &sender, uint8_t* buffer, size_t bufferSize, size_t &length, unsigned long timeoutMs) {
  unsigned long startTime = millis();

  // Controlla se ci sono abbastanza dati disponibili per il sender e la lunghezza
  while (Serial.available() < 2) {
    if (millis() - startTime >= timeoutMs) {
      return false; // Timeout raggiunto
    }
  }

  // Leggi il sender
  sender = Serial.read();

  // Leggi la lunghezza del pacchetto
  byte lengthByte = Serial.read();
  length = (size_t)lengthByte;

  // Verifica se la lunghezza è maggiore del buffer disponibile
  if (length > bufferSize) {
    return false; // Buffer non sufficiente
  }

  // Aspetta che tutti i dati siano disponibili con timeout
  startTime = millis(); // Reimposta il timer
  while (Serial.available() < length) {
    if (millis() - startTime >= timeoutMs) {
      return false; // Timeout raggiunto
    }
  }

  // Leggi i dati
  Serial.readBytes(buffer, length);

  return true; // Pacchetto ricevuto correttamente
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


  // =========== RICEZIONE SERIALE ==========
  byte sender;
  uint8_t data[256]; // Buffer per i dati
  size_t length;
  if (receiveFromWebNexus(sender, data, sizeof(data), length, 1000)) {
    // Pacchetto ricevuto correttamente
    handleReceivedPacket(sender, data, length);
  } else {
    prtn("Timeout o errore nella ricezione del pacchetto.");
  }
  // =======================================
}



// Gestione nuovi messaggi
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {

    String text = bot.messages[i].text;

    if (text == "/accendi") {
      digitalWrite(LED_PIN, HIGH);
      bot.sendMessage(TELEGRAM_CHAT_ID, "LED acceso!", "");
    } else if (text == "/spegni") {
      digitalWrite(LED_PIN, LOW);
      bot.sendMessage(TELEGRAM_CHAT_ID, "LED spento!", "");
    } else {
      bot.sendMessage(TELEGRAM_CHAT_ID, "Comando non riconosciuto. Usa /accendi o /spegni.", "");
    }
  }
}
