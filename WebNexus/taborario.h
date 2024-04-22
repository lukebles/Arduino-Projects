#ifndef TABORARIO_H
#define TABORARIO_H

#include "incomune.h"

#define TABLE_SIZE 24 // 24 ore memorizzate

struct DateTimeValue {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    long sumValues; // Somma dei valori ricevuti
};
DateTimeValue table[TABLE_SIZE];
int currentIndex = -1; // Indica l'indice corrente, -1 se la tabella è vuota

void fillHorsColumns(long receivedValue) {
    time_t cnow = now();
    uint16_t cyear = year(cnow);
    uint8_t cmonth = month(cnow);
    uint8_t cday = day(cnow);
    uint8_t chour = hour(cnow);

    for (int i = 0; i <= currentIndex; i++) {
        if (table[i].year == cyear && table[i].month == cmonth &&
            table[i].day == cday && table[i].hour == chour ) {
            table[i].sumValues += receivedValue;
            return;
        }
    }

    if (currentIndex + 1 == TABLE_SIZE) {
        for (int i = 0; i < TABLE_SIZE - 1; i++) {
            table[i] = table[i + 1];
        }
        currentIndex--;
    }

    currentIndex++;
    table[currentIndex] = {cyear, cmonth, cday, chour, receivedValue};

}

// Sostituisci MAX_TABLE_SIZE con il numero massimo di elementi in table
#define MAX_TABLE_SIZE 40
// Sostituisci MAX_HTML_SIZE con una stima della dimensione massima dell'HTML che genererai
#define MAX_HTML_SIZE 2048

// Funzione per generare la tabella HTML
void generateHTMLTable(char* html) {
    // Inizializza l'HTML con l'intestazione della tabella
    snprintf(html, MAX_HTML_SIZE, "<table border=\"1\"><tr><th>Data/Ora</th><th>Somma dei Valori</th></tr>");

    for (int i = 0; i <= currentIndex && i < MAX_TABLE_SIZE; i++) {
        // Calcola la lunghezza attuale della stringa HTML per evitare di sovrascrivere i dati
        size_t currentLength = strlen(html);

        // Costruisci la stringa della data/ora e la riga della tabella
        char row[128]; // Assicurati che questo sia abbastanza grande per la riga più lunga che potresti generare
        snprintf(row, sizeof(row), "<tr><td>%04d/%02d/%02d %02d:00</td><td>%ld</td></tr>", 
                 table[i].year, table[i].month, table[i].day, table[i].hour, table[i].sumValues);

        // Aggiungi la riga all'HTML, controllando di non superare la dimensione massima
        if (currentLength + strlen(row) < MAX_HTML_SIZE - 1) {
            strcat(html, row);
        } else {
            // Non c'è abbastanza spazio per aggiungere un'altra riga
            break;
        }
    }

    // Chiude la tabella
    strcat(html, "</table>");
}

#endif TABORARIO_H