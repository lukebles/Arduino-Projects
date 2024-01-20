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
  for (int i = 0; i < 1000; i++) {
    // Genera due numeri casuali tra 0 e 500
    int x = random(501);
    int y = random(501);

    String myString = String(x) + "," + String(y) + "\n";
    // Invia le coordinate sulla porta SoftwareSerial
     //digitalWrite(LED_BUILTIN, HIGH);
    Serial.println(myString);
     //digitalWrite(LED_BUILTIN, LOW);
    //delay(10);
  }

  //delay(10);
  // Invia la stringa "FINE" sulla SoftwareSerial
  Serial.print("FINE\n");

  // Attendi 5 secondi (5000 millisecondi)
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
}
