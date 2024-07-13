#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <map>

const char* ssid = "wkm";
const char* password = "12345678";

ESP8266WebServer server(80);

int tiddlerCount = 0;

// Pin configurazione
const int buttonPin = 0; // GPIO0 per il tasto 2
const int ledPin = 2;

unsigned long buttonPressTime = 0;
String morseCode = "";
String decodedMessage = "";

const int debounceDelay = 25; 
unsigned long lastDebounceTimeButton = 0;
bool lastButtonState = HIGH;
bool buttonState = HIGH;

unsigned long buttonPressStartTime = 0;
bool buttonPressed = false;

// Mappa dei codici Morse
// lo spazio è <BT> -...-
std::map<String, String> morseMap = {
  {".-", "A"}, {"-...", "B"}, {"-.-.", "C"}, {"-..", "D"}, {".", "E"},
  {"..-.", "F"}, {"--.", "G"}, {"....", "H"}, {"..", "I"}, {".---", "J"},
  {"-.-", "K"}, {".-..", "L"}, {"--", "M"}, {"-.", "N"}, {"---", "O"},
  {".--.", "P"}, {"--.-", "Q"}, {".-.", "R"}, {"...", "S"}, {"-", "T"},
  {"..-", "U"}, {"...-", "V"}, {".--", "W"}, {"-..-", "X"}, {"-.--", "Y"},
  {"--..", "Z"}, {"-----", "0"}, {".----", "1"}, {"..---", "2"}, {"...--", "3"},
  {"....-", "4"}, {".....", "5"}, {"-....", "6"}, {"--...", "7"}, {"---..", "8"},
  {"----.", "9"}, {".-.-.", "<AR>"}, {"-...-", " "}, {"---...", "<OS>"},
  {".-.-.-", "."}, {"--..--", ","}, {"..--..", "?"}, {"-.-.--", "!"},
  {"-..-.", "/"}, {"-.--.", "("}, {"-.--.-", ")"}, {".----.", "'"}, {"-....-", "-"},
  {".-..-.", "\""}, {"...-..-", "$"}, {".--.-.", "@"}, {"-.--.", "{"}, {"-.--.-", "}"}
};

void setupWiFi() {
  Serial.println("Configurazione WiFi come Access Point...");
  
  // Impostazione del WiFi come Access Point
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Access Point configurato. IP address: ");
  Serial.println(IP);
}

void setup() {
  Serial.begin(115200);

  // WiFi spento inizialmente
  WiFi.mode(WIFI_OFF);

  // Inizializza LittleFS
  if (!LittleFS.begin()) {
    Serial.println("Errore durante il montaggio di LittleFS");
    return;
  }
  Serial.println("LittleFS montato con successo");

  // Carica il contatore dei tiddler
  loadTiddlerCount();

  // Configura il server web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("Server HTTP avviato");

  // Configura il pin del pulsante
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin,OUTPUT);
  digitalWrite(ledPin,HIGH);

}

void loop() {
  server.handleClient();

}

void handleButton2() {
  bool readingButton2 = digitalRead(buttonPin);
  if (readingButton2 != lastButtonState) {
    lastDebounceTimeButton = millis();
  }
  if ((millis() - lastDebounceTimeButton) > debounceDelay) {
    if (readingButton2 != buttonState) {
      buttonState = readingButton2;
      if (buttonState == LOW) {
        buttonPressTime = millis();
      } else {
        unsigned long pressDuration = millis() - buttonPressTime;
        if (pressDuration > 5000) {
          digitalWrite(ledPin,LOW);
          delay(30);
          digitalWrite(ledPin,HIGH);
          setupWiFi();
        } else if (pressDuration > 3000) {
          digitalWrite(ledPin,LOW);
          delay(30);
          digitalWrite(ledPin,HIGH);
          appendDecodedMessageToFile();
        } else {
          processMorseCode(pressDuration);
        }
        buttonPressTime = 0;
      }
    }
  }
  lastButtonState = readingButton2;

  // Decodifica Morse
  if (buttonPressTime == 0 && millis() - lastDebounceTimeButton > 300 && morseCode.length() > 0) {
    decodeMorseCode();
  }
}

void processMorseCode(unsigned long pressDuration) {
  static int dotCount = 0;
  if (pressDuration > 200) {
    morseCode += "-";
    dotCount = 0; // Reset contatore punti
  } else {
    morseCode += ".";
    dotCount++;
    if (dotCount == 7) {
      if (decodedMessage.length() > 0) {
        decodedMessage.remove(decodedMessage.length() - 1);
      }
      dotCount = 0; // Reset contatore punti
      morseCode = ""; // Reset del codice Morse corrente
    }
  }
}

void decodeMorseCode() {
  if (morseMap.find(morseCode) != morseMap.end()) {
    decodedMessage += morseMap[morseCode];
    Serial.println("Carattere decodificato: " + morseMap[morseCode]);
  } else {
    Serial.println("Codice Morse non riconosciuto: " + morseCode);
  }
  morseCode = "";
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

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP8266 TiddlyWiki</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            display: flex;
            flex-direction: column;
            height: 100vh;
            margin: 0;
        }
        .tiddler {
            border: 1px solid #ccc;
            margin: 10px;
            padding: 10px;
        }
        #tiddlers {
            flex: 1;
            overflow-y: auto;
        }
        #newTiddler {
            width: 100%;
            height: 100px;
        }
        #inputContainer {
            display: flex;
            flex-direction: column;
            align-items: stretch;
        }
        button {
            width: 100%;
        }
    </style>
</head>
<body>
    <h1>ESP8266 TiddlyWiki</h1>
    <div id="tiddlers">
)rawliteral";

  File file = LittleFS.open("/tiddlers.txt", "r");
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      html += "<p class='tiddler'>" + line + "</p>";
    }
    file.close();
  }

  html += R"rawliteral(
    </div>
    <div id="inputContainer">
        <textarea id="newTiddler"></textarea><br>
        <button onclick="addTiddler()">Aggiungi Tiddler</button>
    </div>
    <script>
      function addTiddler() {
          var tiddlersDiv = document.getElementById('tiddlers');
          var newTiddlerContent = document.getElementById('newTiddler').value;
          var now = new Date();
          var days = ["DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"];
          var dayOfWeek = days[now.getDay()];
          var timestamp = dayOfWeek + " " +
                          now.getFullYear() + "-" +
                          ("0" + (now.getMonth() + 1)).slice(-2) + "-" +
                          ("0" + now.getDate()).slice(-2) + " " +
                          ("0" + now.getHours()).slice(-2) + ":" +
                          ("0" + now.getMinutes()).slice(-2) + ":" +
                          ("0" + now.getSeconds()).slice(-2);

          var tiddlerCount = Number(localStorage.getItem('tiddlerCount')) || 0;
          tiddlerCount++;
          localStorage.setItem('tiddlerCount', tiddlerCount);

          var newTiddler = "Tiddler " + tiddlerCount + " - " + timestamp + " - " + newTiddlerContent;
          var p = document.createElement('p');
          p.className = 'tiddler';
          p.textContent = newTiddler;
          tiddlersDiv.appendChild(p);
          document.getElementById('newTiddler').value = '';

          // Salva il tiddler lato server
          var xhr = new XMLHttpRequest();
          xhr.open("POST", "/save", true);
          xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
          xhr.send("tiddler=" + encodeURIComponent(newTiddler) + "&count=" + tiddlerCount);
      }
    </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("tiddler") && server.hasArg("count")) {
    String tiddler = server.arg("tiddler");
    tiddlerCount = server.arg("count").toInt();

    File file = LittleFS.open("/tiddlers.txt", "a");
    if (file) {
      file.println(tiddler);
      file.close();
      saveTiddlerCount();
      server.send(200, "text/plain", "Tiddler salvato!");
    } else {
      server.send(500, "text/plain", "Impossibile aprire il file per l'append");
    }
  } else {
    server.send(400, "text/plain", "Richiesta non valida");
  }
}

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
