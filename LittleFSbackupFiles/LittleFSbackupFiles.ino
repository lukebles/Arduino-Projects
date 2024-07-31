#include <LittleFS.h>
#include <FS.h>

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("Errore di montaggio del filesystem");
    return;
  }

  Serial.println("Tentativo di recupero dei dati dal filesystem:");

  // Prova a leggere i file uno ad uno per cercare di recuperare i dati
  Dir dir = LittleFS.openDir("/data");
  while (dir.next()) {
    String fileName = dir.fileName();
    File file = dir.openFile("r");
    if (file) {
      Serial.print("Recupero del file: ");
      Serial.println(fileName);
      Serial.println("Contenuto:");
      while (file.available()) {
        Serial.write(file.read());
      }
      file.close();
      Serial.println("\n-----");
    } else {
      Serial.print("Impossibile aprire il file: ");
      Serial.println(fileName);
    }
  }

  Serial.println("Fine del tentativo di recupero dei dati.");
}

void loop() {
  // Non c'è bisogno di fare nulla qui
}
