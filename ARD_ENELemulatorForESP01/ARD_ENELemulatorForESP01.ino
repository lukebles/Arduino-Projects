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

uint16_t invertiByte(uint16_t val) {
  // Usa lo shift a destra (>>) per spostare i byte meno significativi
  // nella posizione dei byte più significativi e viceversa, con lo shift a sinistra (<<).
  // Poi usa l'operatore OR (|) per combinare i due valori.
  return (val >> 8) | (val << 8);
}

void loop() {
  
  for(int i = 0; i < numSamples; i++) {
    int waveValue = sinusoidalWave(i);
    countWA += waveValue;
    
    DataPacket mystructure_value = {1, countWA, countWR};
    uint8_t mystructure_buffer[sizeof(DataPacket)];
    LkArraylize<DataPacket> mystructure_o;
    mystructure_o.arraylize(mystructure_buffer, mystructure_value);
    
    String hexstring = "";
    for (uint8_t i = 0; i < sizeof(DataPacket); i++){
      if(mystructure_buffer[i] < 0x10) {
        hexstring += '0';
      }      
      hexstring += String(mystructure_buffer[i], HEX);
    }
    hexstring += "\n";        // end message
    
    Serial.print(hexstring);  // sending the message to the PC

    //////////////////////////////////////////

    // String miaStringa = hexstring;
    // char hestx[11];
    // byte barray[5]; 

    // strncpy(hestx, miaStringa.c_str(), 10);
    // hestx[10] = '\0'; // Assicurati che la stringa sia terminata correttamente

    // // Conversione da stringa esadecimale a array di byte
    // for(int i = 0; i < 10; i += 2) {
    //     // Converte ogni coppia di caratteri esadecimali (hestx[i] e hestx[i+1]) in un byte
    //     // e lo memorizza in barray[i/2]
    //     unsigned int byte;
    //     sscanf(&hestx[i], "%2x", &byte); // Legge due caratteri esadecimali e li converte in un intero
    //     barray[i / 2] = (byte) & 0xFF; // Assicura che il valore sia nel range di un byte
    // }

    // LkArraylize<DataPacket> dataPacketConverter;
    // DataPacket receivedData = dataPacketConverter.deArraylize(barray);

    // Serial.println(miaStringa);
    // Serial.println(hestx);
    // Serial.println((receivedData.countActiveWh));

    
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


