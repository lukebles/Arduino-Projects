#ifndef SERIAL_PACKET_HANDLER_H
#define SERIAL_PACKET_HANDLER_H

struct __attribute__((packed)) DataPacket {
    uint16_t activeDiff;
    uint16_t reactiveDiff;
    unsigned long timeDiff;
};

bool serialdatapacket_ready() {
    if (Serial.available()) {
        if (Serial.peek() == 0xFF) { // Controlla il byte di sincronizzazione senza consumarlo
            Serial.read(); // Consuma il byte di sincronizzazione
            
            unsigned long startTime = millis();
            while (Serial.available() < sizeof(DataPacket)) {
                if (millis() - startTime > 100) {
                    return false; // Esce se non riceve i dati completi in tempo
                }
            }

            if (Serial.available() >= sizeof(DataPacket)) {
                return true;
            }
        } else {
            // Consuma byte non necessari
            Serial.read();
        }
    }
    return false;
}

DataPacket read_serialdatapacket() {
    DataPacket packet;
    if (Serial.available() >= sizeof(DataPacket)) {
        Serial.readBytes((uint8_t*)&packet, sizeof(packet));
    }
    return packet;
}

#endif // SERIAL_PACKET_HANDLER_H
