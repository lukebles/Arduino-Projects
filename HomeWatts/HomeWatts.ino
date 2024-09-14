#include <SevSeg.h>

SevSeg sevseg; // Crea un'istanza della classe SevSeg

byte numDigits = 4;
byte digitPins[] = {13, 14, 15, 16}; 
byte segmentPins[] = {3, 5, 6, 7, 8, 9, 10, 12};
bool resistorsOnSegments = false; 
byte hardwareConfig = COMMON_CATHODE; 
bool updateWithDelays = false; 
bool leadingZeros = false;

char buffer[5] = "    "; // Buffer per conservare gli ultimi 4 caratteri ricevuti
int bufferIndex = 0;

unsigned long lastActivityTime = 0; // Variabile per tracciare l'ultimo tempo di attività
bool idleMode = false; // Modalità inattiva

void setup() {
  // Inizializza il display a 7 segmenti
  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros);
  sevseg.setBrightness(1);

  // Imposta la velocità della comunicazione seriale
  Serial.begin(115200);
}

void loop() {
  unsigned long currentTime = millis();

  // Controlla se ci sono dati disponibili sulla seriale
  if (Serial.available() > 0) {
    // Leggi il carattere dalla seriale
    char c = Serial.read();
    lastActivityTime = currentTime; // Aggiorna il tempo dell'ultima attività
    idleMode = false; // Esci dalla modalità inattiva

    // Se riceviamo un newline, consideriamo il messaggio completo e lo visualizziamo
    if (c == '\n') {
      // Riempimento con spazi vuoti per allineare a destra il testo
      int padding = numDigits - bufferIndex;
      for (int i = bufferIndex - 1; i >= 0; i--) {
        buffer[i + padding] = buffer[i];
      }
      for (int i = 0; i < padding; i++) {
        buffer[i] = ' ';
      }

      // Visualizza i caratteri sul display a 7 segmenti
      sevseg.setChars(buffer);
      bufferIndex = 0; // Reset del buffer index per il prossimo messaggio
      for (int i = 0; i < 4; i++) {
        buffer[i] = ' '; // Puliamo il buffer
      }
    } else {
      // Se il carattere non è newline, lo aggiungiamo al buffer
      if (bufferIndex < 4) {
        buffer[bufferIndex] = c;
        bufferIndex++;
      }
    }
  }

  // Controlla se è passato un minuto dall'ultima attività
  if ((currentTime - lastActivityTime) > 3000 && !idleMode) {
    // Visualizza "----" sul display e avvio della modalità inattiva
    sevseg.setChars("----");
    idleMode = true;
  }

  // Aggiorna il display
  sevseg.refreshDisplay();
}
