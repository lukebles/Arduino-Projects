/*
flkdjlfksjdlf ksjldfk jsldkfj sldkfjsldkfj slkdfj lskdj flskdjflksdjf
sldkfslkdfjlskdjf lskhf kajgjdagdsfkjhdslghkagKJSDHIUEHKDJBFNXBCM

*/

//                                                     ---------------
//                                                     | ATMEGA328P  |
//      |---------|     12 pins                        | w/          |
// |||--| 8 8 8 8 |-------------- LED DISPLAY --------<| Arduino     |
//      |---------|                                    | boot        |
//                                                     | loader      |
//                                                     |             |
//                           -----> sensor_pin ------->|             |
//                                                     |             |
//      |--------------|                               |             |
// GND  |              |                               |             |
// |||--| radio module |                               |             |
//      |           TX |--------< transmit_pin -------<|             |
//      |--------------|                               |             |
//                                                     |-------------|


// ==========================
// libraries
// ==========================


#include <LkRadioStructure.h>
#include <LkMultivibrator.h> 
LkMultivibrator timeTxRadio(13000,ASTABLE);
LkMultivibrator tmr_alternanza(3000,MONOSTABLE);
LkMultivibrator tmr_chk_consumo(5000,MONOSTABLE); 
LkMultivibrator tmr_1minuto(60000,ASTABLE); // 1 minuto
#include "SevSeg.h"
SevSeg sevseg; //Instantiate a seven segment controller object


// ==========================
// pins
// ==========================

const int transmit_pin  = 12;
const int receive_pin = 19; // inutilizzato
const int pin_pushbutton = 17; 
const int pin_pulse = 3; // pin with interrupt available

const int ptt_inverted = false;
const int speed = 2000;

// sender e destinatario
#define MYSELF 2     // 

struct txData1{
  uint8_t sender;
  uint16_t countH2O;
};

LkRadioStructure<txData1> txData1_o;

uint16_t counter = 0;
uint16_t valore_precedente_contatore = 0;
long differenza = 0;
uint16_t counter_radio = 0;
bool show_other_value = false;
long minuti = 0;
uint16_t tempo_velocita = 0;
uint16_t tempo_velocita_old = 0;
bool no_consumption = true;

char empty_text[5];
// ====================
// setup
// ====================
void setup() {
  // configurazione ingressi e uscite
  for (int i = 0; i < 20; i++){
    pinMode(i, INPUT_PULLUP);
  }
  pinMode(pin_pulse, INPUT);

  // interrupt conteggio impulsi
  attachInterrupt(digitalPinToInterrupt(pin_pulse),interruptReceived , RISING);

  // configurazione display a led
  byte numDigits = 4;
  byte digitPins[] = {7, 8, 9, 10}; // 
  byte segmentPins[] = {4, 5, 6, 13, 14, 15, 16, 11};
  bool resistorsOnSegments = false; // 'false' means resistors are on digit pins
  byte hardwareConfig = COMMON_CATHODE; // See README.md for options
  bool updateWithDelays = false; // Default. Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros);
  sevseg.setBrightness(1); // value 1-100:  refresh frequency

  // configurazione radio
  txData1_o.globalSetup(speed, transmit_pin, receive_pin);

  tmr_alternanza.start();
  tmr_alternanza.stop();
  tmr_chk_consumo.start();
  tmr_chk_consumo.stop();

  tmr_1minuto.stop();
  tmr_1minuto.start();

  strcpy(empty_text, "    ");
  //Serial.begin(115200);
}


// ====================
// loop
// ====================
void loop(void) {
  float decilitri = 0.0;
  int dl = 0;

  
  if (timeTxRadio.expired()){
    /**********************************************/
    // E' IL MOMENTO DELLA TRASMISSIONE RADIO

    // spegne il display
    sevseg.setChars(empty_text);
    // spengo le scritte sul display
    // prima di inviare il segnale via radio
    sevseg.refreshDisplay(); // **** Must run repeatedly
    
    txData1 sendvalue;
    sendvalue.sender = MYSELF;
    sendvalue.countH2O = counter_radio;
    txData1_o.sendStructure(sendvalue);

    // inizia il timer di 3 secondi
    // dopo che invia il messaggio via radio
    tmr_alternanza.start();
    // abilita la visualizzazione del secondo valore 
    // il consumo dell'ultima apertura
    // di rubinetto
    show_other_value = true;
  } else {
    /**********************************************/
    // SITUAZIONE NORMALE

    // mostra alternativamente due valori
    if (show_other_value){
      if (no_consumption){
        // se non c'è consumo di acqua
        // mostra l'ultimo consumo
        decilitri = float(differenza)/4.0;
        dl = int(decilitri);
      } else {
        // se c'è consumo di acqua mostra
        // la portata in litri/sec
        float diff = float(tempo_velocita - tempo_velocita_old)/1000.0;
        float valo = 1.0/diff*0.25*60;
        dl = int(valo);
      }
    } else {
      // consumi totali
      decilitri = float(counter)/4.0;
      dl = int(decilitri);
      if (dl > 9999){
        counter = 0;
      }
      // // velocita acqua
      // float diff = float(tempo_velocita - tempo_velocita_old)/1000.0;
      // float valo = 1.0/diff*0.25*60;
      // dl = int(valo);
    }  
    sevseg.setNumber(dl, 1); // 1 = posti decimali
    sevseg.refreshDisplay(); // **** Must run repeatedly
  }

  // reset del contatore del display a 4 cifre LED
  if (digitalRead(pin_pushbutton) == LOW){
    // spegne il display
    sevseg.setChars(empty_text);
    sevseg.refreshDisplay(); // **** Must run repeatedly
    
    delay(500);
    // resetta il timer delle ore
    tmr_1minuto.stop();
    tmr_1minuto.start();
    minuti = 0;

    // resetta il contatore
    reset();
  }

  // il timer di 3 secondi è terminato
  if(tmr_alternanza.expired()){
    // disabilita la visualizzazione
    // del secondo valore
    show_other_value = false;
  }

  // non c'è più consumo?
  if (tmr_chk_consumo.expired()){
    // solo la prima volta lo considera "expired"
    // le volte successive non è più "expired"

    // non c'è più consumo:
    no_consumption = true;
    // memorizzo la differenza di conteggi
    differenza = counter - valore_precedente_contatore;
    // aggiorno al nuovo valore precedente
    valore_precedente_contatore = counter;
  }

  //resetta i valori dopo 24 ore
  if (tmr_1minuto.expired()){
    minuti +=1;
    if (minuti >= 60*24){
      minuti = 0;
      reset();
    }
  }

}

void reset(){
    counter = 0;
    valore_precedente_contatore = 0;
    differenza = 0;
}

void interruptReceived(){ 
  tempo_velocita_old = tempo_velocita;
  tempo_velocita = millis();
  // incremento dei contatori
  counter_radio += 1;
  counter += 1;
  // c'è consumo di acqua: reset del timer
  no_consumption = false;
  tmr_chk_consumo.stop();
  tmr_chk_consumo.start();

}
