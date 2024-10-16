#ifndef DATA_HANDLING_H
#define DATA_HANDLING_H

#include <Arduino.h>
#include <TimeLib.h>
#include <WebSocketsServer_Generic.h>

#define MAX_DATA_POINTS 31 // numero di elementi di JSON
#define MAX_LEN_JSON_ROW 100

#define MAX_KEY_LENGTH 20
#define MAX_VALUE_LENGTH 50
#define MAX_CONFIG_ITEMS 10

struct __attribute__((packed)) packet_RadioRxEnergy {
    byte sender;
    uint16_t activeCount;
    uint16_t reactiveCount;
};


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

void fillTable_istant(uint16_t diff_a, uint16_t diff_r, unsigned long timediff) {
  // Calcola la potenza attiva e reattiva
  float activePower = (diff_a * 3600.0f) / (timediff / 1000.0f);
  float reactivePower = (diff_r * 3600.0f) / (timediff / 1000.0f);

  // Arrotonda la potenza
  activePower = roundToTens(activePower);
  reactivePower = roundToTens(reactivePower);

  // Inserisce i valori calcolati nella struttura dati
  istantPoints[idxIstantTable].activePower = activePower;
  istantPoints[idxIstantTable].reactivePower = reactivePower;
  istantPoints[idxIstantTable].timestamp = now();
  istantPoints[idxIstantTable].timeDiff = timediff;

  // Aggiorna l'indice per il prossimo inserimento
  idxIstantTable++;

  // Verifica se l'indice ha raggiunto il massimo
  if (idxIstantTable == MAX_DATA_POINTS) {
      // Sposta tutti i dataPoints verso sinistra di 1
      for (int i = 1; i < MAX_DATA_POINTS; i++) {
          istantPoints[i - 1] = istantPoints[i];
      }
      // Ripristina l'ultimo indice per consentire l'inserimento del nuovo valore
      idxIstantTable = MAX_DATA_POINTS - 1;
  }

  // imposta la potenza anche per l'unità "megane" che 
  setPotenza(activePower);
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
    char arrayJson[MAX_DATA_POINTS * MAX_LEN_JSON_ROW];  
    int pos = 0;  // Posizione corrente nell'array `arrayJson`
    char timestamp[25];  // Buffer per il timestamp
    char activeEnergyStr[7];  // Buffer per l'energia attiva (massimo 6 cifre + terminatore)
    char reactiveEnergyStr[7];  // Buffer per l'energia reattiva

    // Inizializza l'array JSON con il carattere di apertura "["
    pos += snprintf(arrayJson + pos, sizeof(arrayJson) - pos, "[");

    // Itera sui punti dati
    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        // Chiama la funzione che formatta il timestamp e lo salva nel buffer `timestamp`
        formatTime_hours(hoursPoints[i].timestampH, timestamp, sizeof(timestamp));

        // Converte i valori di energia attiva e reattiva in stringhe
        snprintf(activeEnergyStr, sizeof(activeEnergyStr), "%d", hoursPoints[i].diff_a);
        snprintf(reactiveEnergyStr, sizeof(reactiveEnergyStr), "%d", hoursPoints[i].diff_r);

        // Aggiunge al buffer principale la stringa JSON per il singolo punto dati
        pos += snprintf(arrayJson + pos, sizeof(arrayJson) - pos,
                        "{\"activeEnergy\":%s,\"reactiveEnergy\":%s,\"timestamp\":\"%s\"}",
                        activeEnergyStr, reactiveEnergyStr, timestamp);

        // Aggiungi una virgola se non è l'ultimo punto dati
        if (i < MAX_DATA_POINTS - 1) {
            pos += snprintf(arrayJson + pos, sizeof(arrayJson) - pos, ",");
        }
    }

    // Chiudi l'array JSON con il carattere di chiusura "]"
    pos += snprintf(arrayJson + pos, sizeof(arrayJson) - pos, "]");

    // Invia il JSON via WebSocket
    webSocket.sendTXT(num, arrayJson);
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
    // Array di char per contenere il JSON completo.
    char arrayJson[3000];  

    // Array di char per contenere il timestamp formattato.
    char timestamp[25];  

    // Array di char per contenere i valori di energia attiva e reattiva. 
    // La dimensione 6 è sufficiente per contenere valori a 5 cifre più il terminatore null.
    char activeEnergy[7];  
    char reactiveEnergy[7];  

    // Variabile per tracciare la posizione corrente in `arrayJson` in cui aggiungere nuovi caratteri.
    int pos = 0;

    // Inizializza l'array JSON con il carattere di apertura di un array "[".
    // `snprintf` scrive la stringa direttamente in `arrayJson` e ritorna il numero di caratteri scritti.
    // Iniziamo a scrivere da `arrayJson + pos` e aggiorniamo `pos` di conseguenza.
    pos += snprintf(arrayJson + pos, sizeof(arrayJson) - pos, "[");

    // Ciclo che scorre tutti i punti di dati (MAX_DATA_POINTS).
    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        // Funzione che formatta il timestamp in una stringa e la mette nell'array `timestamp`.
        // `sizeof(timestamp)` è la dimensione dell'array (25).
        formatTime_days(daysPoints[i].timestampD, timestamp, sizeof(timestamp));

        // Converte il valore `diff_a` (energia attiva) in un array di caratteri `activeEnergy`.
        // Il formato "%d" indica che `diff_a` è un intero. La dimensione dell'array è gestita automaticamente.
        snprintf(activeEnergy, sizeof(activeEnergy), "%d", daysPoints[i].diff_a);

        // Converte il valore `diff_r` (energia reattiva) in un array di caratteri `reactiveEnergy`.
        snprintf(reactiveEnergy, sizeof(reactiveEnergy), "%d", daysPoints[i].diff_r);

        // Costruisce l'oggetto JSON per il punto dati corrente e lo aggiunge all'array `arrayJson`.
        // Aggiunge le chiavi `activeEnergy`, `reactiveEnergy` e `timestamp` con i rispettivi valori.
        pos += snprintf(arrayJson + pos, sizeof(arrayJson) - pos,
                        "{\"activeEnergy\":%s,\"reactiveEnergy\":%s,\"timestamp\":\"%s\"}",
                        activeEnergy, reactiveEnergy, timestamp);

        // Se non siamo all'ultimo elemento, aggiungiamo una virgola dopo l'oggetto JSON.
        if (i < MAX_DATA_POINTS - 1) {
            pos += snprintf(arrayJson + pos, sizeof(arrayJson) - pos, ",");
        }
    }

    // Aggiunge il carattere di chiusura dell'array JSON "]" alla fine.
    snprintf(arrayJson + pos, sizeof(arrayJson) - pos, "]");

    // Invia il JSON tramite WebSocket al client specificato da `num`.
    webSocket.sendTXT(num, arrayJson);
}


////////////////////////

void sendClient_istant(uint8_t num) {
    // Definisci la dimensione corretta del buffer per contenere l'intero JSON
    char json[3200];  // Puoi calcolare la dimensione in modo simile a prima in base ai dati
    int pos = 0;  // Posizione corrente nell'array `json`
    char timestamp[20];  // Buffer per il timestamp
    char activePowerStr[7];  // Buffer per la potenza attiva
    char reactivePowerStr[7];  // Buffer per la potenza reattiva
    char timeDiffStr[10];  // Buffer per il timeDiff

    // Inizializza l'array JSON con il carattere di apertura "["
    pos += snprintf(json + pos, sizeof(json) - pos, "[");

    // Itera sui punti dati
    for (int i = 0; i < MAX_DATA_POINTS; i++) {
        // Formatta il timestamp
        formatTime_istant(istantPoints[i].timestamp, timestamp, sizeof(timestamp));

        // Converte i valori numerici in stringhe
        snprintf(activePowerStr, sizeof(activePowerStr), "%d", istantPoints[i].activePower);
        snprintf(reactivePowerStr, sizeof(reactivePowerStr), "%d", istantPoints[i].reactivePower);
        snprintf(timeDiffStr, sizeof(timeDiffStr), "%d", istantPoints[i].timeDiff);

        // Costruisce la stringa JSON per il punto dati corrente
        pos += snprintf(json + pos, sizeof(json) - pos,
                        "{\"activePower\":%s,\"reactivePower\":%s,\"timeDiff\":%s,\"timestamp\":\"%s\"}",
                        activePowerStr, reactivePowerStr, timeDiffStr, timestamp);

        // Aggiungi una virgola se non è l'ultimo punto dati
        if (i < MAX_DATA_POINTS - 1) {
            pos += snprintf(json + pos, sizeof(json) - pos, ",");
        }
    }

    // Chiudi l'array JSON con il carattere di chiusura "]"
    pos += snprintf(json + pos, sizeof(json) - pos, "]");

    // Invia il JSON via WebSocket
    webSocket.sendTXT(num, json);
}


void sendClient_powerLimit(uint8_t num, int powerLimit) {
    char json[50];  // Buffer sufficientemente grande per contenere il JSON (lunghezza stimata)
    
    // Costruisce il JSON utilizzando snprintf
    snprintf(json, sizeof(json), "{\"powerLimit\":%d}", powerLimit);

    // Invia il JSON via WebSocket
    webSocket.sendTXT(num, json);
}


void notifyClients() {
    char buffer[20];      // Buffer per il timestamp
    char bufferlong[30];   // Buffer per il timestamp lungo
    char json[256];        // Buffer per contenere l'intero JSON (dimensione stimata)

    // Formatta i timestamp nei rispettivi buffer
    formatTime_istant(istantPoints[idxIstantTable].timestamp, buffer, sizeof(buffer));
    formatTime_radio(istantPoints[idxIstantTable].timestamp, bufferlong, sizeof(bufferlong));

    // Costruisce il JSON usando snprintf
    snprintf(json, sizeof(json), 
             "{\"activePower\":%d,\"reactivePower\":%d,\"timeDiff\":%d,\"timestampLong\":\"%s\",\"timestamp\":\"%s\"}",
             istantPoints[idxIstantTable].activePower,
             istantPoints[idxIstantTable].reactivePower,
             istantPoints[idxIstantTable].timeDiff,
             bufferlong,
             buffer);

    // Invia il JSON tramite WebSocket
    webSocket.broadcastTXT(json);
}


#endif // DATA_HANDLING_H
