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
#define LUCE_CALDAIA_PIN 3

LkMultivibrator timerContatori(60000,ASTABLE); // timerContatori > timerSanitaria + timerTermo
LkMultivibrator timerSanitaria(20000,MONOSTABLE);
LkMultivibrator timerTermo(20000,MONOSTABLE);
LkMultivibrator timerLuceCaldaiaOn(1000,ASTABLE); // ogni secondo legge se la luce del display della caldaia è acceso
LkMultivibrator tmr_lampeggio(30,MONOSTABLE);

#define SANITARIA_calda 1
#define SANITARIA_fredda 2
#define TERMO_calda 0
#define TERMO_fredda 3

#define NR_SENSORI 4
float previousTemp[NR_SENSORI];
byte stato[NR_SENSORI];
float attualeTemp[NR_SENSORI];

// float temperatura = 25.37;  // Temperatura da inviare
// int16_t temperaturaCodificata = (int16_t)(temperatura * 100);

struct __attribute__((packed)) packet_contatori {
    byte sender; // 1 byte
    uint16_t luceCaldaiaOn; // 2 bytes
    uint16_t dummy; // 2 bytes
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

LkRadioStructure<packet_contatori> radio_contatori;
LkRadioStructure<packet_sanitaria> radio_sanitaria;
LkRadioStructure<packet_termo> radio_termo;

packet_contatori ds_contatori;
packet_sanitaria ds_sanitaria;
packet_termo ds_termo;

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
    pinMode(LUCE_CALDAIA_PIN, INPUT_PULLUP);
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
    timerLuceCaldaiaOn.start(); // ogni secondo verifica se è acceso il led della caldaia
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
    ds_contatori.luceCaldaiaOn = 0;
    ds_contatori.dummy = 0;

    ds_sanitaria.sender = 100;
    ds_sanitaria.sanitCaldaC = (int16_t)(attualeTemp[SANITARIA_calda]*100);
    ds_sanitaria.sanitFreddaC = (int16_t)(attualeTemp[SANITARIA_fredda]*100);;

    ds_termo.sender = 101;
    ds_termo.termoCaldaC = (int16_t)(attualeTemp[TERMO_calda]*100);
    ds_termo.termoFreddaC = (int16_t)(attualeTemp[TERMO_fredda]*100);
}

void loop() {

  if (timerLuceCaldaiaOn.expired()){
    if (!digitalRead(LUCE_CALDAIA_PIN)){
      ds_contatori.luceCaldaiaOn += 1;
      prtn(ds_contatori.luceCaldaiaOn);
    }
  }

	// ==================================
	// ogni 60 secondi invia il resoconto
  // ==================================
 
	if (timerContatori.expired()){
		sensors.requestTemperatures();
	    for (int i=0; i < NR_SENSORI; i++){
	      attualeTemp[i] = sensors.getTempCByIndex(i);
        prtn(attualeTemp[i]);
	    }
    //////////////////////////////////
		//ds_contatori.luceCaldaiaOn = 9999;
		ds_contatori.dummy = 0;		
		radio_contatori.sendStructure(ds_contatori);
    prt("inviate differenze contatori: ");
    prtn(ds_contatori.luceCaldaiaOn);
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



