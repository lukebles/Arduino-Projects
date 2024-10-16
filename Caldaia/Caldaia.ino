#include <LkRadioStructure.h>
#include <LkMultivibrator.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Imposta il pin al quale è collegato il bus OneWire
#define ONE_WIRE_BUS 2

// Inizializza il bus OneWire e la libreria DallasTemperature
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const int LED_PIN = 13;
const int TRANSMIT_PIN = 12;


LkMultivibrator timerRadioTx(30000,ASTABLE);
LkMultivibrator timerStatoTemperr(120000,ASTABLE);

#define SANITARIA_calda 0
#define SANITARIA_fredda 4
#define TERMO_calda 2
#define TERMO_fredda 3
#define AMBIENTE 1

#define TEMPER_COSTANTE 1
#define TEMPER_SALITA 2
#define TEMPER_DISCESA 0 

const int maxSensors = 5;
float minTemp[maxSensors];
float maxTemp[maxSensors];
DeviceAddress sensoreID[maxSensors];

bool statoDisplayPrec = HIGH;

// float temperatura = 25.37;  // Temperatura da inviare
// int16_t temperaturaCodificata = (int16_t)(temperatura * 100);

struct __attribute__((packed)) packet_caldaia {
    byte sender; // 1 byte
    uint16_t sanitariaCount; // 2 bytes
    uint16_t termoCount; // 2 bytes
    byte statoSanitaria; // 0=discesa 1=costante 2=salita
    byte statoTermo; // 0=discesa 1=costante 2=salita
};

LkRadioStructure<packet_caldaia> radio;

float tempSanitCaldaPrec = 20.0;
float tempTermoCaldaPrec = 20.0;

packet_caldaia dataToSend;

void setup() {
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial) {
      ; // aspetta la connessione del porto seriale
  }
  //
  dataToSend.sender = 99;
  Serial.println("setup");
  radio.globalSetup(2000, TRANSMIT_PIN, -1);  // Solo trasmissione
  //
  sensors.requestTemperatures();
  tempSanitCaldaPrec = sensors.getTempCByIndex(SANITARIA_calda);
  tempTermoCaldaPrec - sensors.getTempCByIndex(TERMO_calda);
}

void loop() {
  // ogni 30 secondi invia il resoconto
  if (timerRadioTx.expired()){
    sensors.requestTemperatures(); 
    float diffTemperatureSanitaria = sensors.getTempCByIndex(SANITARIA_calda) - sensors.getTempCByIndex(SANITARIA_fredda);
    float diffTemperatureTermo = sensors.getTempCByIndex(TERMO_calda) - sensors.getTempCByIndex(TERMO_fredda);
    int diffTempSanitInt = (int)round(diffTemperatureSanitaria);
    int diffTempTermoInt = (int)round(diffTemperatureTermo);
    dataToSend.sanitariaCount += diffTempSanitInt;
    dataToSend.termoCount += diffTempTermoInt;
    radio.sendStructure(dataToSend);
  }
  // ogni 2 minuti controlla se la temperatura è in salita o discesa
  if (timerStatoTemperr.expired()){
    // richiede le temperature
    sensors.requestTemperatures();
    // sanitaria: temperature in salita o in discesa?
    float tempAttualeSanit = sensors.getTempCByIndex(SANITARIA_calda);
    float diffSanitaria = tempSanitCaldaPrec - tempAttualeSanit;
    tempSanitCaldaPrec = tempAttualeSanit;
    if (diffSanitaria > 0 ){
      dataToSend.statoSanitaria = 0;
    } else if (diffSanitaria == 0){
      dataToSend.statoSanitaria = 1;
    } else {
      dataToSend.statoSanitaria = 2;
    }
    // termo: temperature in salita o in discesa?
    float tempAttualeTermo = sensors.getTempCByIndex(TERMO_calda);
    float diffTermo = tempTermoCaldaPrec - tempAttualeTermo;
    tempTermoCaldaPrec = tempAttualeTermo;
    if (diffTermo > 0 ){
      dataToSend.statoTermo = 0;
    } else if (diffTermo == 0){
      dataToSend.statoTermo = 1;
    } else {
      dataToSend.statoTermo = 2;
    }
  }
}

