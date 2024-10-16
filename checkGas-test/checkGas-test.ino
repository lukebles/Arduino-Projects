const int analogPin = A0;
const unsigned long readInterval = 1000;      // Intervallo di lettura: 1 secondo in millisecondi
const unsigned long printInterval = 300000;   // Intervallo di stampa: 5 minuti in millisecondi

unsigned long lastReadTime = 0;
unsigned long lastPrintTime = 0;

const int numSections = 30;  // Numero di sezioni, modificabile a piacere

int counts[numSections] = {0}; // Array per contare le occorrenze in ogni sezione

void setup() {
  Serial.begin(115200);
  lastReadTime = millis();
  lastPrintTime = millis();
}

void loop() {
  unsigned long currentTime = millis();

  // Controlla se è tempo di leggere un nuovo valore analogico
  if (currentTime - lastReadTime >= readInterval) {
    lastReadTime += readInterval;

    int analogValue = analogRead(analogPin);
    int section = (analogValue * numSections) / 1024;  // Calcola la sezione corrispondente
    if (section > numSections - 1) section = numSections - 1;             // Assicurati che la sezione sia al massimo 9

    counts[section]++;  // Incrementa il conteggio della sezione
  }

  // Controlla se è tempo di stampare i risultati
  if (currentTime - lastPrintTime >= printInterval) {
    lastPrintTime += printInterval;

    Serial.println("Resoconto dei conteggi di ogni sezione negli ultimi 5 minuti:");
    for (int i = 0; i < numSections; i++) {
      Serial.print("Sezione ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(counts[i]);
    }

    // Reset dei conteggi per il prossimo intervallo di 5 minuti
    for (int i = 0; i < numSections; i++) {
      counts[i] = 0;
    }
  }
}
