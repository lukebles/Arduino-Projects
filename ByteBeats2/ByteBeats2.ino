const int stopButton = 3;
const int startButton = 4;
const int greenLEDPin = 10;
const int redLEDPin = 9;
const int blueLEDPin = 11;
const int potPin = A0; // Pin del potenziometro

// include the library code:
#include <LiquidCrystal.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 8, d4 = 7, d5 = 6, d6 = 5, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

int stato = 0;
int statoPrecedente = 0;

// stato = 0
// arduino è avviato ma si deve accertare che lato PC
// che python sia in ascolto nel modo giusto

// stato = 1
// il sistema è avviato
// visualizza sul display la scritta
// ASCOLTO SU LINE IN
// -===|===- hd:1 
// può ascoltare 
// pulsante premuto

bool primoAvvio = true;

MonostabilePrimoAvvio.stop // 1 secondo
MonostabileWatchdog.stop

void setup() {
  pinMode(stopButton, INPUT_PULLUP);
  pinMode(startButton, INPUT_PULLUP);
  pinMode(greenLEDPin, OUTPUT);
  pinMode(redLEDPin, OUTPUT);
  pinMode(blueLEDPin, OUTPUT);

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("ByteBeats Valk R");

  Serial.begin(115200);
  MonostabilePrimoAvvio.start
}

void loop() {


  if MonostabilePrimoAvvio.scaduto {
    print("ARE_YOU_READY?\n")
    MonostabilePrimoAvvio.start
  }

   // Lettura seriale
  if (Serial.available()) {
      String response = Serial.readStringUntil('\n');
      if response = "I_AM_READY!\n"{
        statoPrecedente = 0
        stato = 1
      }

      if sinistra(response) == "LIVELLO"{
        livello = estraiLivello(response)
      }
  }

  if stato = 1 {
    if statoPrecedente == 0 {
      stato precedente = 1
      // stampa su display riga 1 ASCOLTO SU LINE IN
    }
  }

  if primoAvvio{

  }

  if PULSANTEREC premuto {

  } 

  if PULSANTESTOP premuto {

  }
}
