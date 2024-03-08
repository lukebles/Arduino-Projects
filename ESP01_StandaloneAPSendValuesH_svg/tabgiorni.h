#ifndef TABGIORNI_H
#define TABGIORNI_H

#include "incomune.h"

#define TABLE_SIZE_D 24 // 24 ore memorizzate

struct DayStruct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    long sumValues; // Somma dei valori ricevuti
};
DayStruct tabledays[TABLE_SIZE_D];
int surIdxDay = -1; // Indica l'indice corrente, -1 se la tabella è vuota

void filldaysColumns(long receivedValue) {
    time_t cnow = now();
    uint16_t cyear = year(cnow);
    uint8_t cmonth = month(cnow);
    uint8_t cday = day(cnow);

    for (int i = 0; i <= surIdxDay; i++) {
        if (tabledays[i].year == cyear && tabledays[i].month == cmonth &&
            tabledays[i].day == cday ) {
            tabledays[i].sumValues += receivedValue;
            return;
        }
    }

    if (surIdxDay + 1 == TABLE_SIZE) {
        for (int i = 0; i < TABLE_SIZE_D - 1; i++) {
            tabledays[i] = tabledays[i + 1];
        }
        surIdxDay--;
    }

    surIdxDay++;
    tabledays[surIdxDay] = {cyear, cmonth, cday, receivedValue};

}

// Sostituisci MAX_TABLE_SIZE con il numero massimo di elementi in table
#define MAX_TABLE_SIZE 40
// Sostituisci MAX_HTML_SIZE con una stima della dimensione massima dell'HTML che genererai
#define MAX_HTML_SIZE 2048

// Funzione per generare la tabella HTML
void generateHTMLTableDays(char* html) {
    // Inizializza l'HTML con l'intestazione della tabella
    snprintf(html, MAX_HTML_SIZE, "<table border=\"1\"><tr><th>Data/Ora</th><th>Somma dei Valori</th></tr>");

    for (int i = 0; i <= surIdxDay && i < MAX_TABLE_SIZE; i++) {
        // Calcola la lunghezza attuale della stringa HTML per evitare di sovrascrivere i dati
        size_t currentLength = strlen(html);

        // Costruisci la stringa della data/ora e la riga della tabella
        char row[128]; // Assicurati che questo sia abbastanza grande per la riga più lunga che potresti generare
        snprintf(row, sizeof(row), "<tr><td>%04d/%02d/%02d </td><td>%ld</td></tr>", 
                 tabledays[i].year, tabledays[i].month, tabledays[i].day, tabledays[i].sumValues);

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

#endif TABGIORNI_H