#include <ESP8266WiFi.h>
#include "ThingSpeak.h"
#include "config.h"


WiFiClient client;

#define DEBUG 1 // 0 off 1 on

#if DEBUG
#define prt(x) Serial.print(x)
#define prtn(x) Serial.println(x)
#else
#define prt(x)
#define prtn(x)
#endif

#define POWER_LIMIT 3990
#define DEFAULT_LIMIT 999.0

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

float values[9];
// unsigned long timestamp_values[8];
#define valore_dummy 0
#define meteo_temperature 1
#define meteo_humidity 2
#define meteo_pressure 3
#define h2o_sanit_fredda 4
#define h2o_sanit_calda 5
#define h2o_termo_fredda 6
#define h2o_termo_calda 7
#define enel_potenza 8

int contatore = 0;

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
  values[enel_potenza] = int(round(power));
  //timestamp_values[enel_potenza] = millis();
}

void processGasPacket(uint8_t sender, uint8_t* data, size_t length) {
    // Controlla la lunghezza del pacchetto
    if (length != sizeof(int16_t) * 2) {
        return;
    }

    // Decodifica i dati in modo sicuro
    uint16_t calda, fredda;
    memcpy(&calda, &data[0], sizeof(int16_t));
    memcpy(&fredda, &data[2], sizeof(int16_t));

    float f_calda = float(calda/100.0);
    float f_fredda = float(fredda/100.0);

    // Gestione dei pacchetti in base al sender
    switch (sender) {
        case SENDER_H2O_SANITARIA:
            values[h2o_sanit_calda] = f_calda;
            values[h2o_sanit_fredda] = f_fredda;
            //timestamp_values[h2o_sanit_calda] = millis();
            //timestamp_values[h2o_sanit_fredda] = millis();
            break;

        case SENDER_H2O_TERMOSIFONI:
            values[h2o_termo_calda] = f_calda;
            values[h2o_termo_fredda] = f_fredda;
            //timestamp_values[h2o_termo_calda] = millis();
            //timestamp_values[h2o_termo_fredda] = millis();            
            break;

        default:
            // Caso non riconosciuto
            break;
    }
}

void processMeteoPacket(uint8_t sender, uint8_t* data, size_t length) {

    if (length != sizeof(float)) {
        return;
    }

    float valueA;
    memcpy(&valueA, data, sizeof(float));
    
    switch (sender) {
        case SENDER_METEO_TEMP:
            values[meteo_temperature] = valueA;
            //timestamp_values[meteo_temperature] = millis();
            //sendMeteoUpdate(prev_values.temperature, int_valueA, "temp", "°C");
            break;
        case SENDER_METEO_UMIDITA:
            values[meteo_humidity] = valueA;
            //timestamp_values[meteo_humidity] = millis();
            //sendMeteoUpdate(prev_values.humidity, int_valueA, "umidita", "%");
            break;
        case SENDER_METEO_PRESSIONE:
            values[meteo_pressure] = valueA;
            //timestamp_values[meteo_pressure] = millis();
            //sendMeteoUpdate(prev_values.pressure, int_valueA, "press", "hPa");
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



bool receiveFromMulticatch(byte &sender, uint8_t* buffer, size_t bufferSize, size_t &length, unsigned long timeoutMs) {
  unsigned long startTime = millis();

  // Controlla se ci sono abbastanza dati disponibili per il sender e la lunghezza
  while (Serial.available() < 2) {
    if (millis() - startTime >= timeoutMs) {
      return false; // Timeout raggiunto
    }
  }

  // Leggi il sender
  sender = Serial.read();

  prtn(sender);

  // Leggi la lunghezza del pacchetto
  byte lengthByte = Serial.read();
  length = (size_t)lengthByte;

  prtn(length);

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
  // START
  Serial.begin(115200);
  delay(2000);
  prtn("");
  prtn("Avvio ESP01");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  delay(1000);
  digitalWrite(LED_PIN, HIGH);
  // Connessione al WiFi
  prt("Tentativo di connessione al WiFi");
  client.setTimeout(5000); // Imposta timeout di 5 secondi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // Attiva la riconnessione automatica
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    prt(".");
  }
  // AZZERAMENTO VALORI
  for (int i=1; i <= 8; i++){
    values[i]=DEFAULT_LIMIT;
  }
  ThingSpeak.begin(client);
}

void loop() {

  // si resetta ogni ora
  static unsigned long lastRestart = millis();
  if (millis() - lastRestart > 3600000) { // 1 ora
    prtn("Riavvio programmato");
    ESP.restart();
  }


  static unsigned long prevTime = 0; 
  // ============== CONNESSIONE WIFI ===========
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long reconnectStart = millis();
    prt("Tentativo di connessione al WiFi");
    while (WiFi.status() != WL_CONNECTED && millis() - reconnectStart < 300000) {
      WiFi.reconnect();
      delay(500);
    }
    if (WiFi.status() != WL_CONNECTED) {
      prtn("Riavvio dopo un certo tempo di tentativi");
      ESP.restart(); // Riavvia l'ESP01 se non riesce a riconnettersi
    }
  }
  // ============= THING SPEAK ============
  if ((millis() - prevTime) > 16000) {
    // trascorsi i 15 secondi
    // imposta per thingSpeak i valori 
    // che sono stati ricevuti via radio...
    for (int i = 1; i <= 8; i++){
      if (values[i] != DEFAULT_LIMIT){
        ThingSpeak.setField(i, values[i]);
        // una volta memorizzato per thingSpeak
        // lo segna come "da ricevere via radio"
        //values[i] = DEFAULT_LIMIT;
      }
    }
    // ... e li invia
    int result = ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_API_KEY_WRITE);
    delay(1000);
    if (result != 200) { // 200 indica successo
      prtn("Errore nell'invio a ThingSpeak, codice: " + String(result));
      prtn("Heap disponibile: " + String(ESP.getFreeHeap()) + " byte");
    }
    //ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_API_KEY_WRITE);
    //client.stop();

    if (result == 200) {
      for (int i = 1; i <= 8; i++) {
        values[i] = DEFAULT_LIMIT;
      }
    }

    prevTime = millis();
  }
  // =========== RICEZIONE SERIALE ==========
  byte sender;
  uint8_t data[256]; // Buffer per i dati
  size_t length;

  if (receiveFromMulticatch(sender, data, sizeof(data), length, 1000)) {
    // Pacchetto ricevuto correttamente
    handleReceivedPacket(sender, data, length);
  } //else {
    //while (Serial.available() > 0) Serial.read(); // Pulisci il buffer
    //prtn("Timeout o errore nella ricezione del pacchetto.");
    //prtn("Heap disponibile: " + String(ESP.getFreeHeap()) + " byte");
  //}
}
