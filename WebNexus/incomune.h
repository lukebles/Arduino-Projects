#ifndef INCOMUNE_H
#define INCOMUNE_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <TimeLib.h>

#define TAB_ROWS 24
#define TAB_COLUMNS 5

const unsigned int HTML_SIZE = 12000;
long values[TAB_ROWS];  // Array per memorizzare i valori
#define MAX_TEXT_LENGTH 40           // Imposta la dimensione del buffer secondo le necessità
char serialBuffer[MAX_TEXT_LENGTH];  // Buffer per i dati seriali

#endif // INCOMUNE_H


