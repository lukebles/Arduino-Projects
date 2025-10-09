#include <LkRadioStructure.h>
#include <LkMultivibrator.h>

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define TRANSMIT_PIN 12
#define PTT_PIN 13

LkMultivibrator timerTemperatura(30000,ASTABLE);
LkMultivibrator timerUmidita(10000,MONOSTABLE);
LkMultivibrator timerPressione(10000,MONOSTABLE);


struct __attribute__((packed)) packet_temperatura {
    byte sender;       // 1 byte
    float temperatura; // 2 bytes
};

struct __attribute__((packed)) packet_umidita {
    byte sender; // 1 byte
    float umidita;
};

struct __attribute__((packed)) packet_pressione {
    byte sender; // 1 byte
    float pressione;
};

LkRadioStructure<packet_temperatura> radio_temperatura;
LkRadioStructure<packet_umidita> radio_umidita;
LkRadioStructure<packet_pressione> radio_pressione;

packet_temperatura ds_temperatura;
packet_umidita ds_umidita;
packet_pressione ds_pressione;

#define DEBUG 0

#if DEBUG
	#define prt(x) Serial.print(x);
	#define prtn(x) Serial.println(x);
#else
	#define prt(x) 
	#define prtn(x) 
#endif

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;  // I2C
//Adafruit_BME280 bme(BME_CS); // hardware SPI
//Adafruit_BME280 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK); // software SPI


void setup() {
	  // impostazioe piedini
    pinMode(PTT_PIN, OUTPUT);
    digitalWrite(PTT_PIN, HIGH);// alimenta il trasmettitore
    // seriale
    Serial.begin(115200);

    if (!bme.begin(0x76, &Wire)) {
      prtn("Could not find a valid BME280 sensor, check wiring!");
      while (1);
    }

    delay(1500);
    prtn();
    prtn("start");
    // radio
    radio_temperatura.globalSetup(2000, TRANSMIT_PIN, -1);  // Solo trasmissione
    // stop monostabili
    timerUmidita.stop();
    timerUmidita.stop();
    timerPressione.stop();
    // avvio astabile
    timerTemperatura.start(); 

    // impostazione iniziale dei vari valori-radio
    ds_temperatura.sender = 106;
    ds_umidita.sender = 107;
    ds_pressione.sender = 108;

    //
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF   );
}

void loop() {
  float temp = 0;
	// ogni 60 secondi invia il resoconto

	if (timerTemperatura.expired()){
    bme.takeForcedMeasurement();
    temp = bme.readTemperature();
    ds_temperatura.temperatura = temp;
		radio_temperatura.sendStructure(ds_temperatura);
    prt("inviate temperatura ");
    prtn(temp);
    // avvio ritardato delle altre trasmissioni radio
    timerUmidita.start();
	}
	if(timerUmidita.expired()){
    bme.takeForcedMeasurement();
    temp = bme.readHumidity();
    ds_umidita.umidita = temp;
		radio_umidita.sendStructure(ds_umidita);
    prt("inviate umidita ");
    prtn(temp);
		// successiva tx
		timerPressione.start(); // quello dopo
	}
	if (timerPressione.expired()){
    bme.takeForcedMeasurement();
    temp = bme.readPressure();
    temp = temp / 100.0;
    ds_pressione.pressione = temp;
		radio_pressione.sendStructure(ds_pressione);
    prt("inviate pressione ");
    prtn(temp);
	}
}


