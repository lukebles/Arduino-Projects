// #include <SoftwareSerial.h>

// Definisci i pin RX e TX per la SoftwareSerial
// Sostituisci i numeri dei pin con quelli che intendi usare
// int rxPin = 10; // Pin RX
// int txPin = 11; // Pin TX

// Crea un oggetto SoftwareSerial
// SoftwareSerial Serial(rxPin, txPin);

void setup() {
  // Inizia la comunicazione SoftwareSerial a 9600 bps
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  delay(5000);
}

int contatore = 0;

void loop() {
  do {
    // Genera due numeri casuali tra 0 e 500
    int y = random(1024);

    String myString = String(y) + "\n";
    // Invia le coordinate sulla porta SoftwareSerial
     //digitalWrite(LED_BUILTIN, HIGH);
    Serial.print(myString);
     //digitalWrite(LED_BUILTIN, LOW);
    // digitalWrite(LED_BUILTIN, HIGH);

    contatore += 1;

    if (contatore < 10){
      delay(950); // funzionamento regolare
    } else {
      contatore = 0;
      delay(950); // simula una mancata ricezione, ritardando l'invio successivo
    }
    
    // digitalWrite(LED_BUILTIN, LOW);
  } while (true);


}