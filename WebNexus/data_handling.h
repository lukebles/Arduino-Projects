#ifndef DATA_HANDLING_H
#define DATA_HANDLING_H

#include <Arduino.h>
#include <TimeLib.h>
#include <WebSocketsServer_Generic.h>

#define MAX_DATA_POINTS 31

#define MAX_KEY_LENGTH 20
#define MAX_VALUE_LENGTH 50
#define MAX_CONFIG_ITEMS 10

struct ConfigItem {
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
};
ConfigItem configuraz[MAX_CONFIG_ITEMS];

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

extern uint8_t idxIstantTable;
extern WebSocketsServer webSocket;

uint8_t idxIstantTable = MAX_DATA_POINTS - 1;
uint8_t idxHourTable = MAX_DATA_POINTS - 1;
uint8_t idxDayTable  = MAX_DATA_POINTS - 1;

float roundToTens(float value) {
    return round(value / 10) * 10;
}

const char* getWeekdayAbbreviation(int wday) {
    static const char* weekdays[] = {"DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"};
    return weekdays[wday - 1];  // La funzione weekday() di TimeLib restituisce valori da 1 (domenica) a 7 (sabato)
}

const char* getMonthAbbreviation(int month) {
    static const char* months[] = {
        "GEN", "FEB", "MAR", "APR", "MAG", "GIU",
        "LUG", "AGO", "SET", "OTT", "NOV", "DIC"
    };
    return months[month - 1]; // Mese 1-12, array 0-11
}

///////////// SAVE / LOAD CONGURAZIONE ////////

// void saveConfig() {
//   File file = LittleFS.open("/config.ini", "w");
//   if (!file) {
//       Serial.println("Failed to open config file for writing");
//       return;
//   }

//   for (int i = 0; i < MAX_CONFIG_ITEMS; i++) {
//       if (strlen(configuraz[i].key) > 0) {
//           file.print(configuraz[i].key);
//           file.print("=");
//           file.println(configuraz[i].value);
//       }
//   }

//   file.close();
//   LittleFS.end();
//   Serial.println("Configuration saved");
// }

// void loadConfig() {
//     File file = LittleFS.open("/config.ini", "r");
//     if (!file) {
//         Serial.println("Failed to open config file for reading");
//         return;
//     }

//     int index = 0;
//     while (file.available() && index < MAX_CONFIG_ITEMS) {
//         String line = file.readStringUntil('\n');
//         int separatorIndex = line.indexOf('=');
//         if (separatorIndex == -1) continue;

//         String key = line.substring(0, separatorIndex);
//         String value = line.substring(separatorIndex + 1);

//         key.trim();
//         value.trim();

//         strncpy(configuraz[index].key, key.c_str(), MAX_KEY_LENGTH);
//         strncpy(configuraz[index].value, value.c_str(), MAX_VALUE_LENGTH);

//         index++;
//     }

//     file.close();
//     LittleFS.end();
//     Serial.println("Configuration loaded");
// }

///////////// SAVE / LOAD DATA ////////////////

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

void loadData() {
    File file = LittleFS.open("/data.bin", "r");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    file.read((uint8_t*)istantPoints, sizeof(istantPoints));
    file.read((uint8_t*)hoursPoints, sizeof(hoursPoints));
    file.read((uint8_t*)daysPoints, sizeof(daysPoints));

    file.close();
    Serial.println("Data loaded successfully");
}

///////////////////// FORMAT TIME //////////////

void formatTime_radio(time_t timestamp, char* buffer, size_t len) {
    snprintf(buffer, len, "%s %02d/%02d/%04d %02d:%02d:%02d", 
     getWeekdayAbbreviation(weekday(timestamp)),
      day(timestamp), 
      month(timestamp), 
      year(timestamp), 
      hour(timestamp), 
      minute(timestamp),
      second(timestamp)
      );
}

void formatTime_istant(time_t timestamp, char* buffer, size_t len) {
    snprintf(buffer, len, "%02d:%02d:%02d", hour(timestamp), minute(timestamp), second(timestamp));
}

void formatTime_hours(time_t timestamp, char* buffer, size_t len) {
    snprintf(buffer, len, "%s %02d/%02d/%04d %02d:%02d", 
     getWeekdayAbbreviation(weekday(timestamp)),
      day(timestamp), 
      month(timestamp), 
      year(timestamp), 
      hour(timestamp), 
      0);
}

void formatTime_days(time_t timestamp, char* buffer, size_t len) {
  snprintf(buffer, len, "%02d %s %04d %s", 
      day(timestamp), 
      getMonthAbbreviation(month(timestamp)),
      year(timestamp),
      getWeekdayAbbreviation(weekday(timestamp))
  );
}

void extractDateTime(time_t timestamp, int *year, int *month, int *day, int *hour) {
    // Converte il timestamp in una struttura tm
    time_t t = timestamp;
    tmElements_t tm;
    breakTime(t, tm);

    // Estrai anno, mese, giorno, ora, minuto, secondo
    *year = tmYearToCalendar(tm.Year);  // tm.Year è il numero di anni dal 1970
    *month = tm.Month;
    *day = tm.Day;
    *hour = tm.Hour;
}

/////////////////// ISTANTE //////////

void fillTable_istant(uint16_t diff_a, uint16_t diff_r, uint16_t timediff){
  // calcola la potenza
  float activePower = (diff_a * 3600.0) / (timediff / 1000.0);
  float reactivePower = (diff_r * 3600.0) / (timediff / 1000.0);
  activePower = roundToTens(activePower);
  reactivePower = roundToTens(reactivePower);
  //
  istantPoints[idxIstantTable].activePower = activePower;
  istantPoints[idxIstantTable].reactivePower = reactivePower;
  istantPoints[idxIstantTable].timestamp = now();
  istantPoints[idxIstantTable].timeDiff = timediff;
  idxIstantTable = idxIstantTable + 1;
  if (idxIstantTable == MAX_DATA_POINTS) {
      // Sposta tutti i dataPoints verso sinistra di 1
      for (int i = 1; i < MAX_DATA_POINTS; i++) {
          istantPoints[i - 1] = istantPoints[i];
      }
      // Ripristina l'ultimo indice per consentire l'inserimento del nuovo valore
      idxIstantTable = MAX_DATA_POINTS - 1;
  }
}

//////////////// ORA //////////////

void fillTable_hours(uint16_t diff_a, uint16_t diff_r) {
    time_t cnow = now();
    uint16_t cyear = year(cnow);
    uint8_t cmonth = month(cnow);
    uint8_t cday = day(cnow);
    uint8_t chour = hour(cnow);

    int year, month,day,hour;

    for (int i = 0; i <= idxHourTable; i++) {
        extractDateTime(hoursPoints[i].timestampH, &year, &month, &day, &hour);
        if (year == cyear && month == cmonth &&
            day == cday && hour == chour ) {
            hoursPoints[i].diff_a += diff_a;
            hoursPoints[i].diff_r += diff_r;
            return;
        }
    }

    if (idxHourTable == MAX_DATA_POINTS - 1) {
        for (int i = 0; i < MAX_DATA_POINTS - 1; i++) {
            hoursPoints[i] = hoursPoints[i + 1];
        }
        idxHourTable--;
    }

    idxHourTable++;
    hoursPoints[idxHourTable] = {cnow, diff_a, diff_r};
}

void sendClient_hours(uint8_t num) {
    String json = "[";
    char buffer[25];
    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        formatTime_hours(hoursPoints[i].timestampH, buffer, sizeof(buffer));
        json += "{\"activeEnergy\":" + String(hoursPoints[i].diff_a) +
                ",\"reactiveEnergy\":" + String(hoursPoints[i].diff_r) +
                ",\"timestamp\":\"" + String(buffer) + "\"}";
        if (i < MAX_DATA_POINTS - 1) json += ",";
    }
    json += "]";
    webSocket.sendTXT(num, json);
}

//////////// DAYS /////////////////

void fillTable_days(uint16_t diff_a, uint16_t diff_r) {
    time_t cnow = now();
    uint16_t cyear = year(cnow);
    uint8_t cmonth = month(cnow);
    uint8_t cday = day(cnow);

    int year, month,day,dummy;

    for (int i = 0; i <= idxDayTable; i++) {
        extractDateTime(daysPoints[i].timestampD, &year, &month, &day, &dummy);
        if (year == cyear && month == cmonth &&
            day == cday ) {
            daysPoints[i].diff_a += diff_a;
            daysPoints[i].diff_r += diff_r;
            return;
        }
    }

    if (idxDayTable + 1 == MAX_DATA_POINTS) {
        for (int i = 0; i < MAX_DATA_POINTS - 1; i++) {
            daysPoints[i] = daysPoints[i + 1];
        }
        idxDayTable--;
    }

    idxDayTable++;
    daysPoints[idxDayTable] = {cnow, diff_a, diff_r};
}

void sendClient_days(uint8_t num) {
    String json = "[";
    char buffer[25];
    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        formatTime_days(daysPoints[i].timestampD, buffer, sizeof(buffer));
        json += "{\"activeEnergy\":" + String(daysPoints[i].diff_a) +
                ",\"reactiveEnergy\":" + String(daysPoints[i].diff_r) +
                ",\"timestamp\":\"" + String(buffer) + "\"}";
        if (i < MAX_DATA_POINTS - 1) json += ",";
    }
    json += "]";
    webSocket.sendTXT(num, json);
}

////////////////////////

void sendClient_istant(uint8_t num) {
    String json = "[";
    char buffer[20];
    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        formatTime_istant(istantPoints[i].timestamp, buffer, sizeof(buffer));
        json += "{\"activePower\":" + String(istantPoints[i].activePower) +
                ",\"reactivePower\":" + String(istantPoints[i].reactivePower) +
                ",\"timeDiff\":" + String(istantPoints[i].timeDiff) +
                ",\"timestamp\":\"" + String(buffer) + "\"}";
        if (i < MAX_DATA_POINTS - 1) json += ",";
    }
    json += "]";
    webSocket.sendTXT(num, json);
}

void sendClient_powerLimit(uint8_t num,int powerLimit) {
    String json = "{\"powerLimit\":" + String(powerLimit) + "}";
    webSocket.sendTXT(num, json);
}

void notifyClients() {
    char buffer[20];
    char bufferlong[30];
    
    formatTime_istant(istantPoints[idxIstantTable].timestamp, buffer, sizeof(buffer));
    formatTime_radio(istantPoints[idxIstantTable].timestamp, bufferlong, sizeof(bufferlong));

    String json = "{\"activePower\":" + String(istantPoints[idxIstantTable].activePower) +
                  ",\"reactivePower\":" + String(istantPoints[idxIstantTable].reactivePower) +
                  ",\"timeDiff\":" + String(istantPoints[idxIstantTable].timeDiff) +
                  ",\"timestampLong\":\"" + String(bufferlong) + "\"" +
                  ",\"timestamp\":\"" + String(buffer) + "\"}";

    //Serial.println(json);
    webSocket.broadcastTXT(json);
}

#endif // DATA_HANDLING_H
