#include <LkArraylize.h>
#include "LkHexBytes.h" 

struct DataPacket{
  uint8_t sender;
  uint16_t countActiveWh;
  uint16_t countReactiveWh;
};

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // aspetta la connessione della porta seriale
  }
}

void loop() {
  
  if (Serial.available() > 0) {
    String strSeriale = Serial.readStringUntil('\n');
    // la lunghezza del dato sulla seriale deve essere adeguata a DataPacket
    if (strSeriale.length() == sizeof(DataPacket) * 2){
      // 10 caratteri tipo "01FA4388FE" + 1 (terminatore \0)
      char chararray[sizeof(DataPacket) * 2 + 1];
      // 5 caratteri + 1 per il terminatore nullo
      byte bytearray[sizeof(DataPacket) + 1];
      // converte la stringa in un array di caratteri char
      strncpy(chararray, strSeriale.c_str(), sizeof(chararray) - 1);
      // aggiunge il terminatore stringa null \0
      chararray[sizeof(chararray) - 1] = '\0'; 
      // converto l'array di caratteri in un array di bytes
      LkHexBytes::hexCharacterStringToBytes(bytearray, chararray);
      // dichiaro la classe 
      LkArraylize<DataPacket> dearraylizator;
      // ricompongo i dati secondo la struttura
      DataPacket packet_rx = dearraylizator.deArraylize(bytearray);
      // se il mittente è 1 (ENEL)
      if (packet_rx.sender == 1){
        // stampa il contatore dell'energia attiva
        Serial.println(packet_rx.countActiveWh);
      }
    }
  }

}
