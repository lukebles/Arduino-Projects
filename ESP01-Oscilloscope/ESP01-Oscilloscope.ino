#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>

const char* ssid = "oscillo";
const char* password = "osc12345";

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Funzione per inviare la pagina web
void handleRoot() {
  String htmlPage = R"=====(
<!DOCTYPE html>
<html>
<head>
    <title>Oscilloscopio ESP-01</title>
    <style>
        .slider-container {
            display: flex;
            height: 300px; 
            border: 2px solid #000;
            padding: 10px; 
            box-sizing: border-box; 
        }
        .slider-wrapper {
            display: flex;
            flex-direction: column;
            align-items: center;
            margin: 0 10px; 
        }
        .vertical-slider {
            -webkit-appearance: slider-vertical;
            width: 50px;
            height: 200px;
        }
        .slider-label{
            background: red;
            margin-top: 10px;
            transform: rotate(90deg);
            transform-origin: left;
            white-space: nowrap; 
            overflow: hidden; 
            max-width: 60px;
            text-overflow: ellipsis;
        }

        .time-label {
            background: cyan;
            margin-top: 5px;
            margin-left: 40px;
            transform: rotate(90deg);
            transform-origin: left;
            white-space: nowrap; 
            overflow: hidden; 
            max-width: 60px; 
            text-overflow: ellipsis; 
        }        
    </style>
</head>
<body>
    <div class="slider-container" id="sliderContainer"></div>

    <script>
        var webSocket = new WebSocket("ws://" + window.location.hostname + ":81");
        var lastTimestamps = new Array(10).fill(Date.now());

        webSocket.onmessage = function(event) {
            var now = Date.now();
            var value = parseInt(event.data);
            
            lastTimestamps.shift();  
            lastTimestamps.push(now);

            for (var i = 1; i < 10; i++) {
                var nextSlider = document.getElementById('slider' + (i + 1));
                var currentSlider = document.getElementById('slider' + i);
                var currentLabel = document.getElementById('label' + i);
                var currentTimeLabel = document.getElementById('timeLabel' + i);
                currentSlider.value = nextSlider.value;
                currentLabel.innerText = nextSlider.value;
                currentTimeLabel.innerText = lastTimestamps[i] - lastTimestamps[i - 1] + ' ms'; 
            }

            var lastSlider = document.getElementById('slider10');
            var lastLabel = document.getElementById('label10');
            var lastTimeLabel = document.getElementById('timeLabel10');
            lastSlider.value = value;
            lastLabel.innerText = value;
            lastTimeLabel.innerText = now - lastTimestamps[10] + ' ms'; 
        };


        // Funzione per creare gli slider e le etichette del tempo
        function createSliders() {
        var container = document.getElementById('sliderContainer');
        for (var i = 1; i <= 10; i++) {
        var wrapper = document.createElement('div');
        wrapper.className = 'slider-wrapper';
        wrapper.id = 'slider-wrapper' + i;
            var slider = document.createElement('input');
            slider.type = 'range';
            slider.min = '0';
            slider.max = '255';
            slider.className = 'vertical-slider';
            slider.id = 'slider' + i;
            slider.orient = 'vertical';

            var label = document.createElement('span');
            label.className = 'slider-label';
            label.id = 'label' + i;
            label.innerText = '0';

            var timeLabel = document.createElement('span');
            timeLabel.className = 'time-label';
            timeLabel.id = 'timeLabel' + i;
            timeLabel.innerText = '0 ms';

            wrapper.appendChild(slider);
            wrapper.appendChild(label);
            wrapper.appendChild(timeLabel);

            container.appendChild(wrapper);
        }
    }

    window.onload = createSliders;
</script>
</body>
</html>
)=====";
  server.send(200, "text/html", htmlPage);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // Gestisci qui gli eventi WebSocket se necessario
}


void setup() {
  Serial.begin(115200);
  
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("IP Address: ");
  Serial.println(myIP);

  server.on("/", handleRoot);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  static String incomingData = ""; // Stringa per accumulare i dati ricevuti
  webSocket.loop();
  server.handleClient();

  while (Serial.available() > 0) {
    char receivedChar = (char)Serial.read();
    
    // Controlla se il carattere ricevuto è una nuova linea
    if (receivedChar == '\n') {
      int value = incomingData.toInt(); // Converti la stringa accumulata in un intero
      String message = String(value);
      webSocket.broadcastTXT(message);
      incomingData = ""; // Reset della stringa per i nuovi dati
    } else {
      incomingData += receivedChar; // Aggiungi il carattere alla stringa
    }
  }
}


