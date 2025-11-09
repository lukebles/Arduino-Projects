#include <RH_ASK.h>
#include <SPI.h>   // richiesto da RH_ASK anche se non usi SPI

// invia sulla seriale tutti i bytes ricevuti via radio sotto forma di 
// coppie di caratteri esadecimali


// === Configurazione ===
// Velocità identica a VirtualWire: 2000 bps
// Pin (modifica se necessario): rxPin, txPin, pttPin, pttInverted
// Qui abilitiamo PTT invertito (es. DR3100), come nel tuo vw_set_ptt_inverted(true)
static const uint16_t BITRATE = 2000;
static const uint8_t  RX_PIN  = 11;  // default RH_ASK su Uno
static const uint8_t  TX_PIN  = 12;  // non usato qui, ma va passato
static const uint8_t  PTT_PIN = 10;  // non usato qui, ma va passato
static const bool     PTT_INV = true;

RH_ASK driver(BITRATE, RX_PIN, TX_PIN, PTT_PIN, PTT_INV);

// LED di segnalazione (come nel tuo esempio)
const int LED_PIN = 13;

// --- helper: nibble -> '0'..'9','A'..'F'
static inline char hexNibble(uint8_t n) {
  return (n < 10) ? ('0' + n) : ('A' + (n - 10));
}

// --- helper: stampa un byte come 2 cifre HEX (zero-padded, maiuscole)
static inline void printHexByte(uint8_t b) {
  Serial.write(hexNibble((b >> 4) & 0x0F));
  Serial.write(hexNibble(b & 0x0F));
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  while (!Serial) { /* per Leonardo/Micro */ }
  //Serial.println(F("setup (RadioHead RH_ASK receiver)"));

  if (!driver.init()) {
    //
    //Serial.println(F("RH_ASK init failed"));
  }
}

void loop() {
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);

  // Non-bloccante: restituisce true se un pacchetto valido è arrivato
  if (driver.recv(buf, &buflen)) {
    //digitalWrite(LED_PIN, HIGH);

    // 2) Stile "coppie HEX senza spazi + newline" (0F9A12... + '\n')
    for (uint8_t i = 0; i < buflen; i++) {
      printHexByte(buf[i]);
    }
    Serial.write('\n');  // fine messaggio

    //digitalWrite(LED_PIN, LOW);
  }
}
