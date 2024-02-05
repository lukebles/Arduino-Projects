// ===========================================================================
// simula lkPythonRadioInterface nel momento in cui riceve i vari contatori
// via radio e invia i suoi valori sulla seriale
// ===========================================================================

#include <LkArraylize.h>
#include <math.h> // Include la libreria matematica per usare la funzione sin()
#include <LkHexBytes.h>

const int numSamples = 20; // Numero totale di campioni per completare un ciclo
const int maxValue = 10; // Valore massimo dell'onda
const float pi = 3.1415926535897932384626433832795; // Definisce Pi

struct DataPacket{
  uint8_t sender;
  uint16_t countActiveWh;
  uint16_t countReactiveWh;
};

uint16_t countWA = 1000;
uint16_t countWR = 2000;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // aspetta la connessione della porta seriale
  }
}

// uint16_t invertiByte(uint16_t val) {
//   // Usa lo shift a destra (>>) per spostare i byte meno significativi
//   // nella posizione dei byte più significativi e viceversa, con lo shift a sinistra (<<).
//   // Poi usa l'operatore OR (|) per combinare i due valori.
//   return (val >> 8) | (val << 8);
// }

void loop() {
  
  for(int i = 0; i < numSamples; i++) {
    int waveValue = sinusoidalWave(i);
    //countWA += waveValue;
    
    // carico i dati nella struttura
    DataPacket packet_tx = {1, countWA, countWR};
    // creo un buffer di bytes pari alla dimensione della struttura
    uint8_t buffer[sizeof(DataPacket)];
    // dichiaro la classe "arraylizzatrice"
    LkArraylize<DataPacket> arraylizator;
    // inserisco in un array di bytes i dati presenti nella struttura
    arraylizator.arraylize(buffer, packet_tx);
    // definisco una stringa vuota
    String hexstring = "";
    for (uint8_t i = 0; i < sizeof(DataPacket); i++){
      // di ogni byte creo una versione testuale
      if(buffer[i] < 0x10) {
        hexstring += '0';
      }      
      // che accodo nella stringa vuota
      hexstring += String(buffer[i], HEX);
    }
    // aggiungo \n come fine riga per la porta seriale
    hexstring += "\n";
    // invio il messaggio sulla seriale 
    Serial.print(hexstring); 

    delay(1000); // simulazione (dovrebbe essere nella realtà 8 secondi)
  }
}

int sinusoidalWave(int sampleIndex) {
  float amplitude = maxValue / 2.0; // Calcola l'ampiezza
  float frequency = 1.0 / numSamples; // Calcola la frequenza
  // Calcola il valore dell'onda sinusoidale aggiustato per oscillare tra 0 e maxValue
  int value = amplitude + (amplitude * sin(2 * pi * frequency * sampleIndex));
  return value;
}


