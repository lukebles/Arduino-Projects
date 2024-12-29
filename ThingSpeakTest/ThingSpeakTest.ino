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


typedef struct {
    float temperature;
    float humidity;
    float pressure;
    float sanitaria_calda;
    float sanitaria_fredda;
    float termo_calda;
    float termo_fredda;
    float potenza;
} SensorValues;

static SensorValues curr_values = {
                                  DEFAULT_LIMIT, DEFAULT_LIMIT, DEFAULT_LIMIT, 
                                  DEFAULT_LIMIT, DEFAULT_LIMIT, DEFAULT_LIMIT, 
                                  DEFAULT_LIMIT, DEFAULT_LIMIT
                                  };

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
  curr_values.potenza = int(round(power));
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
            curr_values.sanitaria_calda = f_calda;
            curr_values.sanitaria_fredda = f_fredda;
            break;

        case SENDER_H2O_TERMOSIFONI:
            curr_values.termo_calda = f_calda;
            curr_values.termo_fredda = f_fredda;
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
            curr_values.temperature = valueA;
            //sendMeteoUpdate(prev_values.temperature, int_valueA, "temp", "°C");
            break;
        case SENDER_METEO_UMIDITA:
            curr_values.humidity = valueA;
            //sendMeteoUpdate(prev_values.humidity, int_valueA, "umidita", "%");
            break;
        case SENDER_METEO_PRESSIONE:
            curr_values.pressure = valueA;
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
  if ((millis() - prevTime) > 16000) {

    setField(1, curr_values.temperature);
    setField(2, curr_values.humidity);
    setField(3, curr_values.pressure);

    setField(4, curr_values.sanitaria_fredda);
    setField(5, curr_values.sanitaria_calda);
    setField(6, curr_values.termo_fredda);
    setField(7, curr_values.termo_calda);

    setField(8, curr_values.potenza);

    ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_API_KEY_WRITE);

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