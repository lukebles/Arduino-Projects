#include <ESP8266WiFi.h>
#include "ThingSpeak.h"
#include "config.h"


WiFiClient client;

#define DEBUG 0 // 0 off 1 on

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

float values[8];
unsigned long timestamp_values[8];
#define meteo_temperature 1
#define meteo_humidity 2
#define meteo_pressure 3
#define h2o_sanit_fredda 4
#define h2o_sanit_calda 5
#define h2o_termo_fredda 6
#define h2o_termo_calda 7
#define enel_potenza 8

int contatore = 1;

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
  timestamp_values[enel_potenza] = millis();
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
            timestamp_values[h2o_sanit_calda] = millis();
            timestamp_values[h2o_sanit_fredda] = millis();
            break;

        case SENDER_H2O_TERMOSIFONI:
            values[h2o_termo_calda] = f_calda;
            values[h2o_termo_fredda] = f_fredda;
            timestamp_values[h2o_termo_calda] = millis();
            timestamp_values[h2o_termo_fredda] = millis();            
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
            timestamp_values[meteo_temperature] = millis();
            //sendMeteoUpdate(prev_values.temperature, int_valueA, "temp", "°C");
            break;
        case SENDER_METEO_UMIDITA:
            values[meteo_humidity] = valueA;
            timestamp_values[meteo_humidity] = millis();
            //sendMeteoUpdate(prev_values.humidity, int_valueA, "umidita", "%");
            break;
        case SENDER_METEO_PRESSIONE:
            values[meteo_pressure] = valueA;
            timestamp_values[meteo_pressure] = millis();
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

  ThingSpeak.begin(client);
}

void setField(uint8_t field, float value){
  if (value != DEFAULT_LIMIT){
    ThingSpeak.setField(field, value);
  }
}
void loop() {

  static unsigned long prevTime = 0; 

  if (WiFi.status() != WL_CONNECTED) {
    //prtn("Connessione persa, tentativo di riconnessione...");
    WiFi.reconnect();
    digitalWrite(LED_PIN, LOW);
    delay(30);
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    prt(".");
  }
  

  // Controlla se ci sono delle differenze
  if ((millis() - prevTime) > 5000) {
    if (contatore > 8){
      contatore = 1;
    }

    if ((millis() - timestamp_values[contatore]) < 120000){
      setField(contatore, values[contatore]);
      ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_API_KEY_WRITE);
    }
    contatore += 1;
    prevTime = millis();
  }

// =========== RICEZIONE SERIALE ==========
  byte sender;
  uint8_t data[256]; // Buffer per i dati
  size_t length;
  if (receiveFromMulticatch(sender, data, sizeof(data), length, 1000)) {
    // Pacchetto ricevuto correttamente
    handleReceivedPacket(sender, data, length);
  } else {
    prtn("Timeout o errore nella ricezione del pacchetto.");
  }
  // =======================================
}




    // float temperature = 25.5; // Sostituisci con il tuo sensore
    // ThingSpeak.setField(1, temperature);
    // ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_API_KEY_WRITE);
    // delay(15000); // Rispetta il limite di 15 secondi
