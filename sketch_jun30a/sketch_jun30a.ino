#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <map>

const int button1Pin = 2; // GPIO2 per il tasto 1 (non utilizzato per il WiFi)
const int button2Pin = 0; // GPIO0 per il tasto 2

unsigned long button1PressTime = 0;
unsigned long button2PressTime = 0;
String morseCode = "";
String decodedMessage = "";

// Mappa dei codici Morse
std::map<String, String> morseMap = {
  {".-", "A"}, {"-...", "B"}, {"-.-.", "C"}, {"-..", "D"}, {".", "E"},
  {"..-.", "F"}, {"--.", "G"}, {"....", "H"}, {"..", "I"}, {".---", "J"},
  {"-.-", "K"}, {".-..", "L"}, {"--", "M"}, {"-.", "N"}, {"---", "O"},
  {".--.", "P"}, {"--.-", "Q"}, {".-.", "R"}, {"...", "S"}, {"-", "T"},
  {"..-", "U"}, {"...-", "V"}, {".--", "W"}, {"-..-", "X"}, {"-.--", "Y"},
  {"--..", "Z"}, {"-----", "0"}, {".----", "1"}, {"..---", "2"}, {"...--", "3"},
  {"....-", "4"}, {".....", "5"}, {"-....", "6"}, {"--...", "7"}, {"---..", "8"},
  {"----.", "9"}, {".-.-.", "<AR>"}, {"-...-", "<BT>"}, {"---...", "<OS>"},
  {".-.-.-", "."}, {"--..--", ","}, {"..--..", "?"}, {"-.-.--", "!"},
  {"-..-.", "/"}, {"-.--.", "("}, {"-.--.-", ")"}, {".----.", "'"}, {"-....-", "-"},
  {".-..-.", "\""}, {"...-..-", "$"}, {".--.-.", "@"}, {"-.--.", "{"}, {"-.--.-", "}"}
};

void setup() {
  Serial.begin(115200);
  
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);

  if (!LittleFS.begin()) {
    Serial.println("Errore di inizializzazione LittleFS");
    return;
  }

  // Leggi e stampa tutte le frasi memorizzate su frasi.txt
  if (LittleFS.exists("/frasi.txt")) {
    File file = LittleFS.open("/frasi.txt", "r");
    if (file) {
      Serial.println("Frasi memorizzate:");
      while (file.available()) {
        String line = file.readStringUntil('\n');
        Serial.println(line);
      }
      file.close();
    } else {
      Serial.println("Errore nell'apertura del file frasi.txt");
    }
  } else {
    Serial.println("Nessuna frase memorizzata trovata.");
  }

  // WiFi spento inizialmente
  WiFi.mode(WIFI_OFF);
}

void loop() {
  // Gestione del tasto 2 per interpretare i caratteri morse e attivare il WiFi AP
  static unsigned long lastPressTime = 0;
  static int dotCount = 0; // Contatore per i punti consecutivi

  if (digitalRead(button2Pin) == LOW) {
    if (button2PressTime == 0) {
      button2PressTime = millis();
    }
  } else {
    if (button2PressTime != 0) {
      unsigned long pressDuration = millis() - button2PressTime;
      if (pressDuration > 5000) {
        // Attiva la modalità WiFi Access Point
        setupWiFi();
      } else if (pressDuration > 3000) {
        // Aggiungi la frase a frasi.txt
        File file = LittleFS.open("/frasi.txt", "a");
        if (!file) {
          Serial.println("Errore nell'apertura del file");
        } else {
          file.println(decodedMessage);
          file.close();
          Serial.println("Frase aggiunta: " + decodedMessage);
          decodedMessage = "";
        }
      } else {
        // Interpreta il carattere morse
        if (pressDuration > 200) {
          morseCode += "-";
          dotCount = 0; // Reset contatore punti
        } else {
          morseCode += ".";
          dotCount++;
          // Controlla se ci sono 7 punti consecutivi
          if (dotCount == 7) {
            if (decodedMessage.length() > 0) {
              decodedMessage.remove(decodedMessage.length() - 1);
              Serial.println("Ultimo carattere eliminato");
            }
            dotCount = 0; // Reset contatore punti
            morseCode = ""; // Reset del codice Morse corrente
          }
        }
        Serial.println("Carattere Morse aggiunto: " + String(pressDuration > 200 ? '-' : '.'));
      }
      button2PressTime = 0;
      lastPressTime = millis();
    }
  }

  // Aggiunge uno spazio tra i caratteri se il tasto non è premuto per un po' di tempo
  if (button2PressTime == 0 && millis() - lastPressTime > 1000 && morseCode.length() > 0) {
    if (morseMap.find(morseCode) != morseMap.end()) {
      decodedMessage += morseMap[morseCode];
      Serial.println("Carattere decodificato: " + morseMap[morseCode]);
    } else {
      Serial.println("Codice Morse non riconosciuto: " + morseCode);
    }
    morseCode = "";
  }
}

void setupWiFi() {
  Serial.println("Configurazione WiFi come Access Point...");
  
  // Impostazione del WiFi come Access Point
  WiFi.softAP("ESP01_AP", "12345678"); // Sostituisci "ESP01_AP" e "12345678" con il SSID e la password desiderati
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Access Point configurato. IP address: ");
  Serial.println(IP);
}
