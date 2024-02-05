#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <TimeLib.h>

const char* ssid = "sid";
const char* password = "pw12345678";

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

unsigned long previousMillis = 0; 
const long interval = 2000; // Intervallo di un secondo


void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // aspetta la connessione della porta seriale
  }  
  WiFi.softAP(ssid, password);

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHTML());
  });

  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // Inizializza la libreria Time
  Serial.println();
  // setSyncProvider(getTime);
  // if (timeStatus() != timeSet) {
  //   Serial.println("Unable to sync with the RTC");
  // } else {
  //   Serial.println("RTC has set the system time");
  // }
}

void loop() {
  server.handleClient();
  webSocket.loop();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    digitalClockDisplay();
    // Esegui azioni ogni secondo senza bloccare il programma
  }  

  // static time_t prevDisplay = 0; // Memorizza l'ultima volta che l'ora è stata visualizzata
  // if (timeStatus() == timeSet && now() != prevDisplay) { // Aggiorna ogni secondo
  //   prevDisplay = now();
  //   //Serial.println(now());
  //   digitalClockDisplay();
  // }
}

// Funzione fittizia per getTime, non farà nulla in questo esempio
// time_t getTime() {
//   return 0; // La sincronizzazione dell'ora effettiva verrà fatta tramite WebSocket
// }

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String dateTime = String((char *)payload).substring(0, length);
    
    // Estrai ogni componente della data e ora dalla stringa
    int year, month, day, hour, minute, second;
    year = dateTime.substring(0, 4).toInt();
    month = dateTime.substring(5, 7).toInt();
    day = dateTime.substring(8, 10).toInt();
    hour = dateTime.substring(11, 13).toInt();
    minute = dateTime.substring(14, 16).toInt();
    second = dateTime.substring(17, 19).toInt();

    // Ajust for the year offset in tmElements_t
    tmElements_t tm;
    tm.Year = year - 1970;
    tm.Month = month;
    tm.Day = day;
    tm.Hour = hour;
    tm.Minute = minute;
    tm.Second = second;

    time_t t = makeTime(tm); // Converti in time_t
    setTime(t); // Imposta l'ora del sistema
    // Serial.println("Data e ora aggiornate via WebSocket");
  }
}

void digitalClockDisplay() {
  // Visualizza l'ora in formato digitale
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}

void printDigits(int digits) {
  // Funzione di utilità per formattare il tempo
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

String getHTML() {
  String html = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>ESP Time Update</title>
    <script>
        document.addEventListener('DOMContentLoaded', function() {
            var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');
            document.getElementById('updateTime').addEventListener('click', function() {
                var now = new Date();
                var dateTimeString = now.getFullYear() + '-' + 
                                     ('0' + (now.getMonth() + 1)).slice(-2) + '-' + 
                                     ('0' + now.getDate()).slice(-2) + ' ' + 
                                     ('0' + now.getHours()).slice(-2) + ':' + 
                                     ('0' + now.getMinutes()).slice(-2) + ':' + 
                                     ('0' + now.getSeconds()).slice(-2);
                webSocket.send(dateTimeString);
            });
        });
    </script>
</head>
<body>
    <h2>ESP-01 Time Update</h2>
    <button id="updateTime">Aggiorna Data/Ora</button>
</body>
</html>
)=====";
  return html;
}


