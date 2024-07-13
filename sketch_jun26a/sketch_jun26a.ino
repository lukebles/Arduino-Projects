#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

const char* ssid = "ESP8266_TiddlyWiki";
const char* password = "12345678";

ESP8266WebServer server(80);

int tiddlerCount = 0;

void setup() {
  Serial.begin(115200);

  // Configura l'ESP8266 come Access Point
  WiFi.softAP(ssid, password);

  // Mostra l'indirizzo IP del Access Point
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Inizializza LittleFS
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  // Carica il contatore dei tiddler
  loadTiddlerCount();

  // Configura il server web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
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
      server.send(500, "text/plain", "Failed to open file for appending");
    }
  } else {
    server.send(400, "text/plain", "Bad Request");
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
