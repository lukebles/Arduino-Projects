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

LkMultivibrator timerContatori(30000,ASTABLE);
LkMultivibrator timerSanitaria(5000,MONOSTABLE);
LkMultivibrator timerTermo(10000,MONOSTABLE);
LkMultivibrator timerAmbiente(15000,MONOSTABLE);

#define SANITARIA_calda 0
#define SANITARIA_fredda 4
#define TERMO_calda 2
#define TERMO_fredda 3
#define AMBIENTE 1

#define T_SALE 2
#define T_SCENDE 0
#define T_COSTANTE 1

const int maxSensors = 5;
DeviceAddress sensoreID[maxSensors];

float previousTemp[5];
byte stato[5];
float attualeTemp[5];

// float temperatura = 25.37;  // Temperatura da inviare
// int16_t temperaturaCodificata = (int16_t)(temperatura * 100);

struct __attribute__((packed)) packet_contatori {
    byte sender; // 1 byte
    uint16_t sanitariaCount; // 2 bytes
    uint16_t termoCount; // 2 bytes
};

struct __attribute__((packed)) packet_sanitaria {
    byte sender; // 1 byte
    byte statoSanitaria; // 0=discesa 1=costante 2=salita
    int16_t sanitCaldaC;
};

struct __attribute__((packed)) packet_termo {
    byte sender; // 1 byte
    byte statoTermo; // 0=discesa 1=costante 2=salita
    int16_t termoCaldaC;
};

struct __attribute__((packed)) packet_ambiente {
    byte sender; // 1 byte
    int16_t ambienteC;
};

LkRadioStructure<packet_contatori> radio_contatori;
LkRadioStructure<packet_sanitaria> radio_sanitaria;
LkRadioStructure<packet_termo> radio_termo;
LkRadioStructure<packet_ambiente> radio_ambiente;

packet_contatori ds_contatori;
packet_sanitaria ds_sanitaria;
packet_termo ds_termo;
packet_ambiente ds_ambiente;

float totaleDifferenzeSanitaria = 0;
float totaleDifferenzeTermo = 0;

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
    radio_contatori.globalSetup(2000, TRANSMIT_PIN, -1);  // Solo trasmissione
    // impostazioni iniziali temperatura
    sensors.requestTemperatures();
    for (int i=0; i < 5; i++){
      attualeTemp[i] = sensors.getTempCByIndex(i);
      previousTemp[i] = attualeTemp[i];
    }
    // impostazione iniziale dei vari valori-radio
    ds_contatori.sender = 99;
    ds_contatori.sanitariaCount = 0;
    ds_contatori.termoCount = 0;

    ds_sanitaria.sender = 100;
    ds_sanitaria.statoSanitaria = T_COSTANTE;
    ds_sanitaria.sanitCaldaC = (int16_t)(attualeTemp[SANITARIA_calda]*100);

    ds_termo.sender = 101;
    ds_termo.statoTermo = T_COSTANTE;
    ds_termo.termoCaldaC = (int16_t)(attualeTemp[TERMO_calda]*100);

    ds_ambiente.sender = 102;
    ds_ambiente.ambienteC = (int16_t)(attualeTemp[AMBIENTE]*100);

}

void loop() {
  // ==================================
	// ogni 30 secondi invia il resoconto
	if (timerContatori.expired()){
		sensors.requestTemperatures();
    for (int i=0; i < 5; i++){
      attualeTemp[i] = sensors.getTempCByIndex(i);
    }
    // ==============================
		// temperature in salita/discesa?
		getStatus(SANITARIA_calda);
		getStatus(TERMO_calda);
		getStatus(AMBIENTE);
    // =====================================
		// cacolo differenze tra CALDA e FREDDA
		float diffSanitaria  = attualeTemp[SANITARIA_calda] - attualeTemp[SANITARIA_fredda]; 
		float diffTermo = attualeTemp[TERMO_calda] - attualeTemp[TERMO_fredda];
		//
		totaleDifferenzeSanitaria += diffSanitaria;
		totaleDifferenzeTermo += diffTermo;
		// impedisce che venga superato il valore float 65535.0
		totaleDifferenzeSanitaria = chkFloat(totaleDifferenzeSanitaria);
		totaleDifferenzeTermo = chkFloat(totaleDifferenzeTermo);
        // ===========================
		// riempimento deti da inviare
		ds_contatori.sanitariaCount = (int16_t)round(totaleDifferenzeSanitaria);
		ds_contatori.termoCount = (int16_t)round(totaleDifferenzeTermo);		
    // ==============
		// invio dei dati
		radio_contatori.sendStructure(ds_contatori);
    // avvio ritardato delle altre trasmissioni radio
    timerSanitaria.start();
    timerTermo.start();
    timerAmbiente.start();
	}
  if(timerSanitaria.expired()){
    ds_sanitaria.statoSanitaria = stato[SANITARIA_calda];
    ds_sanitaria.sanitCaldaC = (int16_t)(attualeTemp[SANITARIA_calda]*100);
    radio_sanitaria.sendStructure(ds_sanitaria);
  }
  if (timerTermo.expired()){
		ds_termo.statoTermo = stato[TERMO_calda];
		ds_termo.termoCaldaC = (int16_t)(attualeTemp[TERMO_calda]*100);
    radio_termo.sendStructure(ds_termo);
  }
  if (timerAmbiente.expired()){
		ds_ambiente.ambienteC = (int16_t)(attualeTemp[AMBIENTE]*100);
    radio_ambiente.sendStructure(ds_ambiente);
  }
}

float chkFloat(float valore){
	float retValue = 0;
	if (valore > 65535.0){
		retValue = valore - 65535.0;
	} else if (valore > 0.0) {
		retValue = valore;
	} else {
		retValue = 0.0;
	}
	return retValue;
}

void getStatus(byte idxTermometro){
    // leggo la temperatura
    // la tengo "buona" finchè non sale o non scende
    // il sale o scende ha una finestra di 0.5°C
    //float attualeTemp = sensors.getTempCByIndex(idxTermometro);
    float diff = previousTemp[idxTermometro] - attualeTemp[idxTermometro];
    float quantita = fabs(diff);
    if (quantita > 0.5){
    	if (diff >= 0){
    		stato[idxTermometro] = T_SCENDE;
		    previousTemp[idxTermometro] = attualeTemp[idxTermometro];
    	} else {
    		// cala
    		stato[idxTermometro] = T_SALE;
		    previousTemp[idxTermometro] = attualeTemp[idxTermometro];
    	}
    } else {
    	// nessun cambiamento
    	stato[idxTermometro] = T_COSTANTE;
    }
}