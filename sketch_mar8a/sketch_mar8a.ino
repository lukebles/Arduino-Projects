struct DateTimeValue {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    long value; // Per memorizzare la somma dei valori ricevuti
};


#include <TimeLib.h>

#define TABLE_SIZE 40
DateTimeValue table[TABLE_SIZE];
int currentIndex = -1; // Indica l'indice corrente, -1 se la tabella è vuota

void addOrUpdateValue(long receivedValue) {
    time_t cnow = now(); // Assume che tu abbia già impostato TimeLib con il tempo corretto
    uint16_t cyear = year(cnow);
    uint8_t cmonth = month(cnow);
    uint8_t cday = day(cnow);
    uint8_t chour = hour(cnow);

    // Cerca un elemento esistente con la stessa data/ora
    for (int i = 0; i <= currentIndex; i++) {
        if (table[i].year == cyear && table[i].month == cmonth &&
            table[i].day == cday && table[i].hour == chour) {
            // Elemento trovato, aggiorna il valore e ritorna
            table[i].value += receivedValue;
            return;
        }
    }

    // Se siamo qui, non è stata trovata una corrispondenza
    // Verifica se la tabella è piena
    if (currentIndex + 1 == TABLE_SIZE) {
        // Coda piena, rimuove l'elemento più vecchio (shift degli elementi)
        for (int i = 0; i < TABLE_SIZE - 1; i++) {
            table[i] = table[i + 1];
        }
        currentIndex--;
    }

    // Inserisce il nuovo elemento
    currentIndex++;
    table[currentIndex].year = cyear;
    table[currentIndex].month = cmonth;
    table[currentIndex].day = cday;
    table[currentIndex].hour = chour;
    table[currentIndex].value = receivedValue;
}

void setup() {
    Serial.begin(9600);
    // Inizializza qui TimeLib se necessario
}

void loop() {
    // Qui potresti leggere i valori dalla seriale e chiamare addOrUpdateValue
}
