#include <LittleFS.h>

#define MAX_DATA_POINTS 31

struct DataInstant {
    uint16_t activePower;
    uint16_t reactivePower;
    time_t timestamp;
    unsigned long timeDiff;
};
extern DataInstant istantPoints[MAX_DATA_POINTS];

struct DataEnergyHours {
    time_t timestampH;
    uint16_t diff_a; 
    uint16_t diff_r; 
};
extern DataEnergyHours hoursPoints[MAX_DATA_POINTS];

struct DataEnergyDays {
    time_t timestampD;
    uint16_t diff_a; 
    uint16_t diff_r; 
};
extern DataEnergyDays daysPoints[MAX_DATA_POINTS];

DataInstant istantPoints[MAX_DATA_POINTS];
DataEnergyHours hoursPoints[MAX_DATA_POINTS];
DataEnergyDays daysPoints[MAX_DATA_POINTS];

void setup() {
    Serial.begin(115200);

    if (!LittleFS.begin()) {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    // Controllo dello spazio disponibile
    FSInfo fs_info;
    LittleFS.info(fs_info);
    Serial.println();
    Serial.print("Total space: ");
    Serial.println(fs_info.totalBytes);
    Serial.print("Used space: ");
    Serial.println(fs_info.usedBytes);

    // Altre inizializzazioni

    saveData();  // Chiamata alla funzione saveData
}

void loop(){

}

void saveData() {
    File file = LittleFS.open("/data.bin", "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    file.write((const uint8_t*)istantPoints, sizeof(istantPoints));
    file.write((const uint8_t*)hoursPoints, sizeof(hoursPoints));
    file.write((const uint8_t*)daysPoints, sizeof(daysPoints));

    file.close();
    Serial.println("Data saved successfully");
}
