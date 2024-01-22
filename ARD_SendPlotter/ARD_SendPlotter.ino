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

void loop() {
  do {
    // Genera due numeri casuali tra 0 e 500
    int y = random(1024);

    String myString = String(y) + "\n";
    // Invia le coordinate sulla porta SoftwareSerial
     //digitalWrite(LED_BUILTIN, HIGH);
    Serial.println(myString);
     //digitalWrite(LED_BUILTIN, LOW);
    // digitalWrite(LED_BUILTIN, HIGH);
    delay(100); // ritardo di 1 secondo per 256 valori all'incirca
    // digitalWrite(LED_BUILTIN, LOW);
  } while (true);


}
