struct __attribute__((packed)) DataPacket {
    uint16_t activeDiff;
    uint16_t reactiveDiff;
    unsigned long timeDiff;
    uint16_t intensitaLuminosa;
};

void setup() {
  Serial.begin(115200); // Avvia la comunicazione seriale integrata
  while (!Serial) {
    ;
  }
}

void loop() {
  if (Serial.available()) {
    if (Serial.peek() == 0xFF) { // Controlla il byte di sincronizzazione senza consumarlo
        Serial.read(); // Consuma il byte di sincronizzazione
        
        unsigned long startTime = millis();
        while (Serial.available() < sizeof(DataPacket)) {
            if (millis() - startTime > 100) {
                return; // Esce se non riceve i dati completi in tempo
            }
        }

        if (Serial.available() >= sizeof(DataPacket)) {
            DataPacket packet;
            Serial.readBytes((uint8_t*)&packet, sizeof(packet));
            Serial.println();
            // Stampa i numeri ricevuti
            Serial.print("Active Diff: ");
            Serial.println(packet.activeDiff);
            Serial.print("Reactive Diff: ");
            Serial.println(packet.reactiveDiff);
            Serial.print("Time Diff: ");
            Serial.println(packet.timeDiff);
            Serial.print("Intensita Luminosa: ");
            Serial.println(packet.intensitaLuminosa);
        } else {
            //Serial.print("Byte disponibili: ");
            //Serial.println(Serial.available());
        }
    } else {
        // Consuma byte non necessari
        Serial.read();
    }
  }
}
