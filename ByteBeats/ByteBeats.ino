const int stopButton = 3;
const int startButton = 4;
const int greenLEDPin = 10;
const int redLEDPin = 9;
const int blueLEDPin = 11;
const int potPin = A0; // Pin del potenziometro

// include the library code:
#include <LiquidCrystal.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 8, d4 = 7, d5 = 6, d6 = 5, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


#define ULTIMO_COMANDO_E_STATO_STOP 1
#define ULTIMO_COMANDO_E_STATO_START 2

int ultimoComando = ULTIMO_COMANDO_E_STATO_STOP;
unsigned long previousMillis = 0; // Per il timer dell'effetto respiro
const unsigned long interval = 20; // Intervallo di aggiornamento della luminosità in ms
unsigned long volumeMillis = 0; // Per l'invio periodico del volume
const unsigned long volumeInterval = 500; // Intervallo di controllo del volume (500 ms)

int brightness = 51; // Luminosità iniziale (20%)
int fadeAmount = 3; // Incremento di luminosità
int previousVolume; // Volume precedente

void setup() {
    pinMode(stopButton, INPUT_PULLUP);
    pinMode(startButton, INPUT_PULLUP);
    pinMode(greenLEDPin, OUTPUT);
    pinMode(redLEDPin, OUTPUT);
    pinMode(blueLEDPin, OUTPUT);

      // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("ByteBeats Valk R");

    Serial.begin(115200);

    // Imposta il valore iniziale di previousVolume leggendo il potenziometro
    int initialPotValue = analogRead(potPin);
    previousVolume = map(initialPotValue, 0, 1023, 0, 100); // Mappa il valore su 0-100

    // Accensione iniziale dei LED
    digitalWrite(greenLEDPin, HIGH);
    delay(300); 
    digitalWrite(greenLEDPin, LOW);

    digitalWrite(redLEDPin, HIGH);
    delay(300);
    digitalWrite(redLEDPin, LOW);

    digitalWrite(blueLEDPin, HIGH);
    delay(300); 
    digitalWrite(blueLEDPin, LOW);
}

void loop() {
    // Controllo pulsanti
    if (digitalRead(stopButton) == LOW) {
        if (ultimoComando == ULTIMO_COMANDO_E_STATO_START) {
            ultimoComando = ULTIMO_COMANDO_E_STATO_STOP;
            Serial.println("STOP");
            delay(500); // Debounce
        }
    } else if (digitalRead(startButton) == LOW) {
        if (ultimoComando == ULTIMO_COMANDO_E_STATO_STOP) {
            ultimoComando = ULTIMO_COMANDO_E_STATO_START;
            Serial.println("START");
            delay(500); // Debounce
        }
    }

    // Effetto respiro
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        // Aggiorna la luminosità
        brightness += fadeAmount;

        // Inverti la direzione del cambiamento di luminosità ai limiti (20% e 80%)
        if (brightness <= 51 || brightness >= 204) {
            fadeAmount = -fadeAmount;
        }

        // Imposta la luminosità del LED in base allo stato
        if (ultimoComando == ULTIMO_COMANDO_E_STATO_START) {
            analogWrite(redLEDPin, brightness);
            analogWrite(blueLEDPin, 0); // Spegni il LED blu
        } else {
            analogWrite(blueLEDPin, brightness);
            analogWrite(redLEDPin, 0); // Spegni il LED rosso
        }
    }

    // Lettura e invio del valore del potenziometro
    if (currentMillis - volumeMillis >= volumeInterval) {
        volumeMillis = currentMillis;
        
        int potValue = analogRead(potPin); // Legge il valore del potenziometro
        int volume = map(potValue, 0, 1023, 0, 100); // Mappa il valore su una scala di 0-100
        int vtrentuno = map(potValue, 0, 1023, 0, 31); 

        // Invia il valore solo se c'è una variazione di almeno il 5%
        if (abs(volume - previousVolume) > 0) {
            analogWrite(greenLEDPin,255);
            lcd.setCursor(0, 1);
            lcd.print("  ");
            lcd.setCursor(0, 1);
            lcd.print(vtrentuno);

            Serial.print("VOLUME:"); // Prefisso per identificare il volume
            Serial.println(volume); // Invia il valore del volume via seriale
            previousVolume = volume; // Aggiorna il valore del volume precedente
            delay(100);
            analogWrite(greenLEDPin,0);
        }
    }

    // Lettura seriale
    if (Serial.available()) {
        String response = Serial.readStringUntil('\n');
        Serial.print("Ricevuto: ");
        Serial.println(response);

        if (response == "FILE_SAVE_CONFIRMED") {
            blinkLED(greenLEDPin);
        } else if (response == "BOOT_OK") {
            blinkLED(blueLEDPin);
        } else if (response == "START_CONFIRMED") {
            blinkLED(redLEDPin);
        }
    }
}

void blinkLED(int ledPin) {
    for (int i = 0; i < 5; i++) {
        digitalWrite(ledPin, HIGH);
        delay(250);
        digitalWrite(ledPin, LOW);
        delay(250);
    }
}
