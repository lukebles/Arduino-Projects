#include <OneWire.h>
#include <DallasTemperature.h>

// Imposta il pin al quale è collegato il bus OneWire
#define ONE_WIRE_BUS 2

// Inizializza il bus OneWire e la libreria DallasTemperature
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

const int delayTime = 10000; // 10 secondi
const int maxSensors = 5; // Cambia questo numero in base a quanti sensori hai
float minTemp[maxSensors];
float maxTemp[maxSensors];

// Array per nomi sensori arbitrari
String nomeSensore[maxSensors] = {"Sensore 1", "Sensore 2", "Sensore 3", "Sensore 4", "Sensore 5"};

// Array per salvare gli ID dei sensori
DeviceAddress sensoreID[maxSensors];

void setup() {
  Serial.begin(9600);
  sensors.begin();
  
  // Imposta i valori iniziali di min e max
  for (int i = 0; i < maxSensors; i++) {
    minTemp[i] = 1000.0; // Imposta un valore alto per iniziare
    maxTemp[i] = -1000.0; // Imposta un valore basso per iniziare
  }
  
  // Salva l'ID di ogni sensore
  for (int i = 0; i < sensors.getDeviceCount(); i++) {
    if (!sensors.getAddress(sensoreID[i], i)) {
      Serial.println("Errore nel recuperare l'ID del sensore!");
    }
  }
}

void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void loop() {
  sensors.requestTemperatures(); // Richiedi la temperatura a tutti i sensori
  for (int i = 0; i < sensors.getDeviceCount(); i++) {
    float temp = sensors.getTempCByIndex(i);

    // Aggiorna il minimo e il massimo
    if (temp < minTemp[i]) {
      minTemp[i] = temp;
    }
    if (temp > maxTemp[i]) {
      maxTemp[i] = temp;
    }

    // Calcola la variazione di temperatura
    float tempVariation = maxTemp[i] - minTemp[i];

    // Stampa i risultati
    Serial.print("Nome: ");
    Serial.print(nomeSensore[i]);
    Serial.print(" - ID: ");
    printAddress(sensoreID[i]);
    Serial.print(" - Temp: ");
    Serial.print(temp);
    Serial.print("°C, Min: ");
    Serial.print(minTemp[i]);
    Serial.print("°C, Max: ");
    Serial.print(maxTemp[i]);
    Serial.print("°C, Variazione: ");
    Serial.print(tempVariation);
    Serial.println("°C");
  }
  
  delay(delayTime); // Attendi 10 secondi
}
