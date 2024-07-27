#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <LittleFS.h>

extern int tiddlerCount; // Dichiarazione esterna del contatore di tiddler
extern String decodedMessage; // Dichiarazione esterna del messaggio decodificato

void loadTiddlerCount() {
  File file = LittleFS.open("/count.txt", "r");
  if (file) {
    tiddlerCount = file.parseInt();
    file.close();
  } else {
    tiddlerCount = 0;
  }
}

void saveTiddlerCount() {
  File file = LittleFS.open("/count.txt", "w");
  if (file) {
    file.println(tiddlerCount);
    file.close();
  }
}

void appendDecodedMessageToFile() {
  File file = LittleFS.open("/tiddlers.txt", "a");
  if (!file) {
    Serial.println("Errore nell'apertura del file");
  } else {
    file.println(decodedMessage);
    file.close();
    Serial.println("Frase aggiunta: " + decodedMessage);
    decodedMessage = "";
  }
}


#endif // FILESYSTEM_H
