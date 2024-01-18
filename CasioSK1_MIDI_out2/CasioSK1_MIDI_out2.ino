#include <MIDI.h>

// 2 Jan 2023: corretto errore sulla nota B4

const int MIDI_C1 = 24;
const int MIDI_C1S = 25;
const int MIDI_D1 = 26;
const int MIDI_D1S = 27;
const int MIDI_E1 = 28;
const int MIDI_F1 = 29;
const int MIDI_F1S = 30;
const int MIDI_G1 = 31;
const int MIDI_G1S = 32;
const int MIDI_A1 = 33;
const int MIDI_A1S = 34;
const int MIDI_B1 = 35;

const int MIDI_C2 = 36;
const int MIDI_C2S = 37;
const int MIDI_D2 = 38;
const int MIDI_D2S = 39;
const int MIDI_E2 = 40;
const int MIDI_F2 = 41;
const int MIDI_F2S = 42;
const int MIDI_G2 = 43;
const int MIDI_G2S = 44;
const int MIDI_A2 = 45;
const int MIDI_A2S = 46;
const int MIDI_B2 = 47;

const int MIDI_C3 = 48;  // Middle C in some definitions
const int MIDI_C3S = 49;
const int MIDI_D3 = 50;
const int MIDI_D3S = 51;
const int MIDI_E3 = 52;
const int MIDI_F3 = 53;  // F3
const int MIDI_F3S = 54; // F#3
const int MIDI_G3 = 55;
const int MIDI_G3S = 56;
const int MIDI_A3 = 57;
const int MIDI_A3S = 58;
const int MIDI_B3 = 59;

const int MIDI_C4 = 60;  // Middle C in some definitions
const int MIDI_C4S = 61;
const int MIDI_D4 = 62;
const int MIDI_D4S = 63;
const int MIDI_E4 = 64;
const int MIDI_F4 = 65;
const int MIDI_F4S = 66;
const int MIDI_G4 = 67;
const int MIDI_G4S = 68;
const int MIDI_A4 = 69; // Tuning standard A (440 Hz)
const int MIDI_A4S = 70;
const int MIDI_B4 = 71;

const int MIDI_C5 = 72;
const int MIDI_C5S = 73;
const int MIDI_D5 = 74;
const int MIDI_D5S = 75;
const int MIDI_E5 = 76;
const int MIDI_F5 = 77;
const int MIDI_F5S = 78;
const int MIDI_G5 = 79;
const int MIDI_G5S = 80;
const int MIDI_A5 = 81;
const int MIDI_A5S = 82;
const int MIDI_B5 = 83;

const int MIDI_C6 = 84;
const int MIDI_C6S = 85;
const int MIDI_D6 = 86;
const int MIDI_D6S = 87;
const int MIDI_E6 = 88;
const int MIDI_F6 = 89;
const int MIDI_F6S = 90;
const int MIDI_G6 = 91;
const int MIDI_G6S = 92;
const int MIDI_A6 = 93;
const int MIDI_A6S = 94;
const int MIDI_B6 = 95;

const int MIDI_C7 = 96;
const int MIDI_C7S = 97;
const int MIDI_D7 = 98;
const int MIDI_D7S = 99;
const int MIDI_E7 = 100;
const int MIDI_F7 = 101;
const int MIDI_F7S = 102;
const int MIDI_G7 = 103;
const int MIDI_G7S = 104;
const int MIDI_A7 = 105;
const int MIDI_A7S = 106;
const int MIDI_B7 = 107;

MIDI_CREATE_DEFAULT_INSTANCE();

const int numKO = 8; // Numero di linee KC
const int numKI = 4; // Numero di linee KI

int KOpins[numKO] = {2, 3, 4, 5, 6, 7, 8, 9}; // Pin di Arduino collegati alle linee KC
int KIpins[numKI] = {10, 11, 12, 13}; // Pin di Arduino collegati alle linee KI

int LED = A1;
int KO_EXTRA = A0; // per leggere due pulsanti ONE KEY PLAY R, ONE KEY PLAY L


int noteMapping[4][8] = {
  // KI5: KC1, KC2, KC3, KC4, KC5, KC6, KC7, KC8
  { MIDI_F3, MIDI_A3, MIDI_C4S, MIDI_F4, MIDI_A4, MIDI_C5S, MIDI_F5, MIDI_A5 },
  // KI6: KC1, KC2, KC3, KC4, KC5, KC6, KC7, KC8
  { MIDI_F3S, MIDI_A3S, MIDI_D4, MIDI_F4S, MIDI_A4S, MIDI_D5, MIDI_F5S, MIDI_A5S },
  // KI7: KC1, KC2, KC3, KC4, KC5, KC6, KC7, KC8
  { MIDI_G3, MIDI_B3, MIDI_D4S, MIDI_G4, MIDI_B4, MIDI_D5S, MIDI_G5, MIDI_B5 },
  // KI8: KC1, KC2, KC3, KC4, KC5, KC6, KC7, KC8
  { MIDI_G3S, MIDI_C4, MIDI_E4, MIDI_G4S, MIDI_C5, MIDI_E5, MIDI_G5S, MIDI_C6}
};

bool keyStates[numKO][numKI] = {{false}};

void setup() {
  MIDI.begin(MIDI_CHANNEL_OMNI);

  // Configura i pin di ingresso
  for (int i = 0; i < numKO; i++) {
    pinMode(KOpins[i], OUTPUT);
    digitalWrite(KOpins[i], LOW);
  }
  
  // Configura i pin di uscita
  for (int i = 0; i < numKI; i++) {
    pinMode(KIpins[i], INPUT);
  }

  pinMode(LED, OUTPUT);
  digitalWrite(LED,HIGH);

  pinMode(KO_EXTRA, OUTPUT);
  digitalWrite(KO_EXTRA,LOW);

}

void loop() {
  for (int i = 0; i < numKO; i++) {
    digitalWrite(KOpins[i], HIGH);
    for (int j = 0; j < numKI; j++) {
      bool currentState = digitalRead(KIpins[j]);

      if (currentState && !keyStates[i][j]) {
        // Tasto premuto per la prima volta
        int note = noteMapping[j][i]; // Ottieni la nota corrispondente
        MIDI.sendNoteOn(note, 127, 1); // Invia MIDI Note On
        keyStates[i][j] = true; // Aggiorna lo stato del tasto
        digitalWrite(LED,HIGH);
      } else if (!currentState && keyStates[i][j]) {
        // Tasto rilasciato
        int note = noteMapping[j][i]; // Ottieni la nota corrispondente
        MIDI.sendNoteOff(note, 0, 1); // Invia MIDI Note Off
        keyStates[i][j] = false; // Aggiorna lo stato del tasto
        digitalWrite(LED,LOW);
      }
    }
    digitalWrite(KOpins[i], LOW);
  }

  // guardo se viene premuto uno dei due tasti speciali
  int tasto = 0;
  digitalWrite(KO_EXTRA, HIGH);
    if (digitalRead(KIpins[0])){
      tasto = 1;
    } else if (digitalRead(KIpins[1])){
      tasto = 2;
    }
  digitalWrite(KO_EXTRA, LOW);
  if (tasto != 0){
      digitalWrite(LED,HIGH);
      delay(1000);
      digitalWrite(LED,LOW);
  }
}
