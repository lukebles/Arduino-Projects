#include <LkRadioStructure.h>
#include <LkMultivibrator.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Imposta il pin al quale è collegato il bus OneWire
#define ONE_WIRE_BUS 2

// Inizializza il bus OneWire e la libreria DallasTemperature
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

#define LED_PIN 5
#define TRANSMIT_PIN 12
#define PTT_PIN 13

LkMultivibrator timerContatori(60000,ASTABLE);
LkMultivibrator timerSanitaria(10000,MONOSTABLE);
LkMultivibrator timerTermo(10000,MONOSTABLE);
LkMultivibrator timerVariazioni(10000,MONOSTABLE);
LkMultivibrator tmr_lampeggio(30,MONOSTABLE);

#define SANITARIA_calda 1
#define SANITARIA_fredda 2
#define TERMO_calda 0
#define TERMO_fredda 3

#define T_SALE 2
#define T_SCENDE 0
#define T_COSTANTE 1

#define NR_SENSORI 4
float previousTemp[NR_SENSORI];
byte stato[NR_SENSORI];
float attualeTemp[NR_SENSORI];

// float temperatura = 25.37;  // Temperatura da inviare
// int16_t temperaturaCodificata = (int16_t)(temperatura * 100);

struct __attribute__((packed)) packet_contatori {
    byte sender; // 1 byte
    uint16_t sanitariaCount; // 2 bytes
    uint16_t termoCount; // 2 bytes
};

struct __attribute__((packed)) packet_sanitaria {
    byte sender; // 1 byte
    int16_t sanitCaldaC;
    int16_t sanitFreddaC;
};

struct __attribute__((packed)) packet_termo {
    byte sender; // 1 byte
    int16_t termoCaldaC;
    int16_t termoFreddaC;
};

struct __attribute__((packed)) packet_variazioni_calde {
    byte sender; // 1 byte
    byte statoTermo;
    byte statoSanitaria;
};

LkRadioStructure<packet_contatori> radio_contatori;
LkRadioStructure<packet_sanitaria> radio_sanitaria;
LkRadioStructure<packet_termo> radio_termo;
LkRadioStructure<packet_variazioni_calde> radio_variazioni_calde;

packet_contatori ds_contatori;
packet_sanitaria ds_sanitaria;
packet_termo ds_termo;
packet_variazioni_calde ds_variazioni_calde;

#define DEBUG 0

#if DEBUG
	#define prt(x) Serial.print(x);
	#define prtn(x) Serial.println(x);
#else
	#define prt(x) 
	#define prtn(x) 
#endif

void setup() {
	  // impostazioe piedini
    pinMode(LED_PIN, OUTPUT);
    pinMode(PTT_PIN, OUTPUT);
    digitalWrite(PTT_PIN, HIGH);// alimenta il trasmettitore
    // seriale
    Serial.begin(115200);
    delay(1500);
    prtn();
    prtn("start");
    // radio
    radio_contatori.globalSetup(2000, TRANSMIT_PIN, -1);  // Solo trasmissione
    // stop monostabili
    timerSanitaria.stop();
    timerTermo.stop();
    timerVariazioni.stop();
    // avvio astabile
    timerContatori.start(); 


    // impostazioni iniziali temperatura
    sensors.requestTemperatures();
    for (int i=0; i < NR_SENSORI; i++){
      attualeTemp[i] = sensors.getTempCByIndex(i);
      previousTemp[i] = attualeTemp[i];
    }
    // impostazione iniziale dei vari valori-radio
    ds_contatori.sender = 99;
    ds_contatori.sanitariaCount = 0;
    ds_contatori.termoCount = 0;

    ds_sanitaria.sender = 100;
    ds_sanitaria.sanitCaldaC = (int16_t)(attualeTemp[SANITARIA_calda]*100);
    ds_sanitaria.sanitFreddaC = (int16_t)(attualeTemp[SANITARIA_fredda]*100);;

    ds_termo.sender = 101;
    ds_termo.termoCaldaC = (int16_t)(attualeTemp[TERMO_calda]*100);
    ds_termo.termoFreddaC = (int16_t)(attualeTemp[TERMO_fredda]*100);

    ds_variazioni_calde.sender = 102;
    ds_variazioni_calde.statoSanitaria = T_COSTANTE;
    ds_variazioni_calde.statoSanitaria = T_COSTANTE;
}

void loop() {
	static float contatoreDiffTempSanitaria = 0;
	static float contatoreDiffTempTermo = 0;	
	// ==================================
	// ogni 60 secondi invia il resoconto
  prtn("Lettura sensori:");
	if (timerContatori.expired()){
		sensors.requestTemperatures();
	    for (int i=0; i < NR_SENSORI; i++){
	      attualeTemp[i] = sensors.getTempCByIndex(i);
        prtn(attualeTemp[i]);
	    }
	  // ==============================
		// temperature in salita/discesa?
		getStatus(SANITARIA_calda);
		getStatus(TERMO_calda);
    // i contatori vengono incrementati
    // delle differenza di temperatura solo se la 
    // temperatura sale
    if (stato[SANITARIA_calda] == T_SALE){
  		float diffSanitaria  = attualeTemp[SANITARIA_calda] - attualeTemp[SANITARIA_fredda];
      diffSanitaria = noNegativi(diffSanitaria);
  		contatoreDiffTempSanitaria += diffSanitaria;
  		contatoreDiffTempSanitaria = chkFloat(contatoreDiffTempSanitaria);
    }
    if (stato[TERMO_calda] == T_SALE){
      float diffTermo = attualeTemp[TERMO_calda] - attualeTemp[TERMO_fredda];
      diffTermo = noNegativi(diffTermo);
      contatoreDiffTempTermo += diffTermo;
      contatoreDiffTempTermo = chkFloat(contatoreDiffTempTermo);
    }
		// riempimento dati da inviare
		ds_contatori.sanitariaCount = (int16_t)round(contatoreDiffTempSanitaria);
		ds_contatori.termoCount = (int16_t)round(contatoreDiffTempTermo);		
		// ==============
		// invio dei dati
		radio_contatori.sendStructure(ds_contatori);
    prtn("inviate differenze contatori");
    //
    lampeggio();
    // avvio ritardato delle altre trasmissioni radio
    timerSanitaria.start();
	}
	if(timerSanitaria.expired()){
		ds_sanitaria.sanitCaldaC = (int16_t)(attualeTemp[SANITARIA_calda]*100);
		ds_sanitaria.sanitFreddaC = (int16_t)(attualeTemp[SANITARIA_fredda]*100);
		radio_sanitaria.sendStructure(ds_sanitaria);
    prtn("inviate temp sanitaria");
		//
		lampeggio();
		// successiva tx
		timerTermo.start(); // quello dopo
	}
	if (timerTermo.expired()){
		ds_termo.termoCaldaC = (int16_t)(attualeTemp[TERMO_calda]*100);
		ds_termo.termoFreddaC = (int16_t)(attualeTemp[TERMO_fredda]*100);
		radio_termo.sendStructure(ds_termo);
    prtn("inviate temp termo");
		//
		lampeggio();
		// successiva tx 
		timerVariazioni.start();
	}
	if (timerVariazioni.expired()){
		ds_variazioni_calde.statoTermo = stato[TERMO_calda];
		ds_variazioni_calde.statoSanitaria = stato[SANITARIA_calda];
		radio_variazioni_calde.sendStructure(ds_variazioni_calde);
    prtn("inviate variazioni");
		  //
		lampeggio();
	}
	//
	if (tmr_lampeggio.expired()){
		digitalWrite(LED_PIN,LOW);
	}
}

void lampeggio(){
	tmr_lampeggio.start();
	digitalWrite(LED_PIN,HIGH);
}

float chkFloat(float valore){
	float retValue;
	if (valore > 65535.0){
		retValue = valore - 65535.0;
	} else if (valore > 0.0) {
		retValue = valore;
	} else {
		retValue = 0.0;
	}
	return retValue;
}

void getStatus(byte indexSensore){
    // leggo la temperatura
    // la tengo "buona" finchè non sale o non scende
    // il sale o scende ha una finestra di 0.5°C
    //float attualeTemp = sensors.getTempCByIndex(indexSensore);
    float diff = previousTemp[indexSensore] - attualeTemp[indexSensore];
    float quantita = fabs(diff);
    if (quantita > 1.0){
    	if (diff >= 0.0){
    		stato[indexSensore] = T_SCENDE;
		    previousTemp[indexSensore] = attualeTemp[indexSensore];
    	} else {
    		// cala
    		stato[indexSensore] = T_SALE;
		    previousTemp[indexSensore] = attualeTemp[indexSensore];
    	}
    } else {
    	// nessun cambiamento: rimane lo stato precedente
      // ( non passiamo ma a "temperatura costante" ma solo "SCENDE" o "SALE")
    	stato[indexSensore] = previousTemp[indexSensore];
      // non aggiorno "previous" con "attuale"
      // perchè altrimenti non avverrebbe mai nessun cambiamento
      // (tra una lettura e l'altra non avremmo quasi mai
      // una differenza di temperatura di 1°C)
    }
}

float noNegativi(float value){
    if (value < 0.0){
      value = 0.0;
    } 
    return value;
}
