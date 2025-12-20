#include <RH_ASK.h>
#include <SPI.h>  // richiesto da RadioHead anche se non usiamo SPI


/*
Usa RadioHead / RH_ASK per ricevere sul pin 11 (D11).
Trasmette subito su Seriale @ 115200 la sequenza di byte ricevuta:
ogni byte → 2 caratteri esadecimali (es. 0A, FF, 3C…),
tutti attaccati, senza spazi,
terminati da \n (newline).
Un LED sul pin 13 lampeggia per 10 ms ogni volta che arriva un pacchetto valido, 
senza usare delay() (quindi senza bloccare la ricezione).
*/
// ================== CONFIG ==================

const int LED_PIN        = 13;
const unsigned long LED_PULSE_MS = 10;   // 1/100 di secondo

// RH_ASK(driver) :
// RH_ASK(uint16_t speed, uint8_t rxPin, uint8_t txPin, uint8_t pttPin)
RH_ASK driver(
  2000,  // bitrate in bps (adatta al tuo trasmettitore: 2000 come nei tuoi sketch)
  11,    // rxPin: QUI entra il segnale dal ricevitore radio (DOUT modulo RF)
  12,    // txPin: non usato, ma deve essere un pin valido
  10     // pttPin: non usato
);

// Stato del lampeggio LED (non bloccante)
bool ledOn = false;
unsigned long ledOnSince = 0;

// ================== SETUP ==================

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(1200);
  // Su Arduino Uno questo while non è necessario, ma non fa danni
  // while (!Serial) {;}

  if (!driver.init()) {
    // Se qualcosa va storto nell'init, lo segnaliamo su seriale
    Serial.println(F("RH_ASK init failed"));
  } else {
    Serial.println(F("RH_ASK receiver ready (RX on D11)"));
  }
}

// ================== FUNZIONE UTILE: stampa byte in HEX ==================

void printByteAsHex(uint8_t b) {
  // Stampa due cifre esadecimali, zero-padded
  char buf[3];
  snprintf(buf, sizeof(buf), "%02X", b);
  Serial.print(buf);
}

// ================== LOOP ==================

void loop() {
  // 1) Controllo LED non bloccante
  if (ledOn) {
    unsigned long now = millis();
    if (now - ledOnSince >= LED_PULSE_MS) {
      digitalWrite(LED_PIN, LOW);
      ledOn = false;
    }
  }

  // 2) Provo a ricevere un pacchetto RadioHead (non blocca)
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);

  if (driver.recv(buf, &buflen)) {
    // Pacchetto valido ricevuto

    // Accendo il LED per 10 ms (non bloccante)
    digitalWrite(LED_PIN, HIGH);
    ledOn = true;
    ledOnSince = millis();

    // Trasformo tutti i byte in coppie di caratteri esadecimali
    // TUTTO ATTACCATO, SENZA SPAZI
    for (uint8_t i = 0; i < buflen; i++) {
      printByteAsHex(buf[i]);
    }

    // Termino la "stringa" con newline
    Serial.print('\n');
  }

  // Nessun delay: lasciamo libero il core per il timing di RH_ASK
}
