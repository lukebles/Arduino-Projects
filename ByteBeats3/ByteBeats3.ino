#include <LiquidCrystal.h>

// Definizione dei pin
const int stopButton = 3;
const int startButton = 4;
const int greenLEDPin = 10;
const int redLEDPin = 9;
const int blueLEDPin = 11;
const int potPin = A0; // Pin del potenziometro

// Inizializzazione del display LCD
const int rs = 12, en = 8, d4 = 7, d5 = 6, d6 = 5, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() {
    // Configurazione dei pin
    pinMode(stopButton, INPUT_PULLUP);
    pinMode(startButton, INPUT_PULLUP);
    pinMode(greenLEDPin, OUTPUT);
    pinMode(redLEDPin, OUTPUT);
    pinMode(blueLEDPin, OUTPUT);

    // Inizializzazione del display
    lcd.begin(16, 2);
    lcd.print("In attesa...");

    // Avvio della comunicazione seriale
    Serial.begin(115200);
}

void loop() {
    // Controllo dei pulsanti e invio del loro stato
    if (digitalRead(stopButton) == LOW) {
        Serial.println("STOP_BUTTON:1");
    } else {
        Serial.println("STOP_BUTTON:0");
    }

    if (digitalRead(startButton) == LOW) {
        Serial.println("START_BUTTON:1");
    } else {
        Serial.println("START_BUTTON:0");
    }

    // Lettura del potenziometro e invio del valore
    int potValue = analogRead(potPin);
    Serial.print("POTENTIOMETER:");
    Serial.println(potValue);

    // Lettura dei comandi dalla seriale
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');

        if (command.startsWith("LED:")) {
            String ledCommand = command.substring(4);
            if (ledCommand == "GREEN_ON") {
                digitalWrite(greenLEDPin, HIGH);
            } else if (ledCommand == "GREEN_OFF") {
                digitalWrite(greenLEDPin, LOW);
            } else if (ledCommand == "RED_ON") {
                digitalWrite(redLEDPin, HIGH);
            } else if (ledCommand == "RED_OFF") {
                digitalWrite(redLEDPin, LOW);
            } else if (ledCommand == "BLUE_ON") {
                digitalWrite(blueLEDPin, HIGH);
            } else if (ledCommand == "BLUE_OFF") {
                digitalWrite(blueLEDPin, LOW);
            }
        } else if (command.startsWith("LCD:")) {
            String lcdCommand = command.substring(4);
            if (lcdCommand.startsWith("LINE1:")) {
                lcd.setCursor(0, 0);
                lcd.print(lcdCommand.substring(6));
            } else if (lcdCommand.startsWith("LINE2:")) {
                lcd.setCursor(0, 1);
                lcd.print(lcdCommand.substring(6));
            }
        }
    }

    delay(500); // Ritardo per evitare flooding della seriale
}
