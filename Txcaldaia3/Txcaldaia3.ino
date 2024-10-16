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

#define SANITARIA_calda 0
#define SANITARIA_fredda 4
#define TERMO_calda 2
#define TERMO_fredda 3
#define AMBIENTE 1

#define T_SALE 2
#define T_SCENDE 0
#define T_COSTANTE 1
#define T_STRANA 3 // se temperatura calda < temp fredda

const int maxSensors = 5;
DeviceAddress sensoreID[maxSensors];

float previousTemp[5];
byte stato[5];

// float temperatura = 25.37;  // Temperatura da inviare
// int16_t temperaturaCodificata = (int16_t)(temperatura * 100);

struct __attribute__((packed)) packet_caldaia {
    byte sender; // 1 byte
    uint16_t sanitariaCount; // 2 bytes
    uint16_t termoCount; // 2 bytes
    byte statoSanitaria; // 0=discesa 1=costante 2=salita
    byte statoTermo; // 0=discesa 1=costante 2=salita
    int16_t ambienteC;
    int16_t sanitCaldaC;
    int16_t termoCaldaC;
};

LkRadioStructure<packet_caldaia> radio;
packet_caldaia dataToSend;

void setup() {
	// impostazioe piedini
    pinMode(LED_PIN, OUTPUT);
    // seriale
    Serial.begin(115200);
    while (!Serial) {
        ; // aspetta la connessione del porto seriale
    }
    Serial.println();
    Serial.println("setup");
    // radio
    radio.globalSetup(2000, TRANSMIT_PIN, -1);  // Solo trasmissione
    // impostazioni iniziali temperatura
    sensors.requestTemperatures();
    previousTemp[SANITARIA_calda] = sensors.getTempCByIndex(SANITARIA_calda);
    previousTemp[TERMO_calda] = sensors.getTempCByIndex(TERMO_calda);
    previousTemp[AMBIENTE] = sensors.getTempCByIndex(AMBIENTE);
    //
    dataToSend.sender = 99;
    dataToSend.sanitariaCount = 0;
    dataToSend.termoCount = 0;
    dataToSend.statoSanitaria = T_COSTANTE;
    dataToSend.statoTermo = T_COSTANTE;
    dataToSend.ambienteC = (int16_t)(sensors.getTempCByIndex(AMBIENTE)*100);
    dataToSend.sanitCaldaC = (int16_t)(sensors.getTempCByIndex(SANITARIA_calda)*100);
    dataToSend.termoCaldaC = (int16_t)(sensors.getTempCByIndex(TERMO_calda)*100);
}

void loop() {
    // ogni 30 secondi invia il resoconto
    if (timerRadioTx.expired()){
      sensors.requestTemperatures();
      // temperature in salita/discesa?
      getStatus(SANITARIA_calda);
      getStatus(TERMO_calda);
      getStatus(AMBIENTE);
      // cacolo differenze tra CALDA e FREDDA
      float diffSanitaria  = sensors.getTempCByIndex(SANITARIA_calda) - sensors.getTempCByIndex(SANITARIA_fredda); 
      float diffTermo = sensors.getTempCByIndex(TERMO_calda) - sensors.getTempCByIndex(TERMO_fredda);
      // conversione in numero intero
      int diffSanitariaI = (int)abs(round(diffSanitaria));
      int diffTermoI = (int)abs(round(diffTermo));
      // riempimento deti da inviare
        dataToSend.sanitariaCount += diffSanitariaI;
        dataToSend.termoCount += diffTermoI;
        dataToSend.statoSanitaria = stato[SANITARIA_calda];
        dataToSend.statoTermo = stato[TERMO_calda];
        dataToSend.ambienteC = (int16_t)(sensors.getTempCByIndex(AMBIENTE)*100);
        dataToSend.sanitCaldaC = (int16_t)(sensors.getTempCByIndex(SANITARIA_calda)*100);
        dataToSend.termoCaldaC = (int16_t)(sensors.getTempCByIndex(TERMO_calda)*100);
        // invio dei dati
      radio.sendStructure(dataToSend);
    }
}

void getStatus(byte idxTermometro){
    // leggo la temperatura
    // la tengo "buona" finchè non sale o non scende
    if (previousTemp[idxTermometro] == sensors.getTempCByIndex(idxTermometro)){
    	stato[idxTermometro] = T_COSTANTE;
    } else if (previousTemp[idxTermometro] < sensors.getTempCByIndex(idxTermometro)){
    	stato[idxTermometro] = T_SALE;
    } else if (previousTemp[idxTermometro] > sensors.getTempCByIndex(idxTermometro)){
    	stato[idxTermometro] = T_SCENDE;
    } else {
    	stato[idxTermometro] = T_STRANA;
    }
    previousTemp[idxTermometro] = sensors.getTempCByIndex(idxTermometro);
}