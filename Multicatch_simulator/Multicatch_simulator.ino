// =================================
// simula la ricezione di pacchetti 
// via radio e li invia via seriale
// ================================

struct __attribute__((packed)) DataPacket {
    uint16_t activeDiff;
    uint16_t reactiveDiff;
    unsigned long timeDiff;
    uint16_t intensitaLuminosa;
};

// Variabili globali per il calcolo della potenza istantanea
unsigned long prevTime = 0;
uint16_t prevActiveCount = 0;
uint16_t prevReactiveCount = 0;
uint16_t intensitaLuminosa = 0;

// Variabili per la simulazione
unsigned long lastEnergyDataTime = 0;
unsigned long lastLightDataTime = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; 
    }
}

uint16_t simulatore_contA = 64000;
uint16_t simulatore_contR = 65500;

void simulateEnergyData() {
    simulatore_contA += random(0, 12);
    simulatore_contR += random(0, 6);    

    unsigned long currentTime = millis();
    if (prevTime != 0) { // Evita il primo calcolo
        unsigned long timeDiff = currentTime - prevTime;
        uint16_t activeDiff = simulatore_contA - prevActiveCount;
        uint16_t reactiveDiff = simulatore_contR - prevReactiveCount;

        //Serial.println(activeDiff);

        DataPacket packet = {activeDiff, reactiveDiff, timeDiff, intensitaLuminosa};
        Serial.write(0xFF); // Byte di sincronizzazione
        Serial.write((uint8_t*)&packet, sizeof(packet));

        prevActiveCount = simulatore_contA;
        prevReactiveCount = simulatore_contR;
        prevTime = currentTime;
    } else {
        prevTime = currentTime;
        prevActiveCount = simulatore_contA;
        prevReactiveCount = simulatore_contR;
    }
}


void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastEnergyDataTime >= 8800) {
        lastEnergyDataTime = currentMillis;
        simulateEnergyData();
    }

}
