#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
#include <TimeLib.h>

// ===================================
// change ssid and password as needed
// ===================================
const char* ssid = "sid";
const char* password = "pw12345678";

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

#include <cstring> // Per usare strcpy e strncpy

void setup() {

  Serial.begin(115200);
  while (!Serial) {
    ; // aspetta la connessione della porta seriale
  }
  Serial.println();

  WiFi.softAP(ssid, password);
  
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", getHome());
  });

  server.on("/ore", HTTP_GET, []() {
    server.send(200, "text/html", getEnergiaOra());
  });

  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  tmElements_t tm;
  
}

void loop() {
  server.handleClient();
  webSocket.loop();
}


void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    String message = String((char *)payload).substring(0, length);
    Serial.println("Messaggio ricevuto: " + message);
  }  
}


String getHome() {
  String html = "<!DOCTYPE html><html>";
  html += "<head>";
  html += getHead();
  html += "</head>";
  html += "<body>";
  html += "<table><tbody><tr>";
  html += "<td><a href=/>potenza</a></td>";
  html += "<td><a href=/ore>energia ore</a></td>";
  html += "<td><a href=/giorni>energia giorni</a></td>";
  html += "<td><a href=/setDateTime>set date time</a></td>";  
  html += "</tr></tbody></table>";
  html += "<h1>Home</h1>";
  html += "</body>";
  html += "<script>";
  html += "var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');";
  html += "function sendButtonEvent(buttonId) {";
  html += "  webSocket.send(buttonId);";
  html += "}";
  html += getScript();
  html += "</script>";
  html += "</html>";
  return html;
}

String getEnergiaOra() {
  String html = "<!DOCTYPE html><html>";
  html += "<head>";
  html += getHead();
  html += "</head>";
  html += "<body>";
  html += "<table><tbody><tr>";
  html += "<td><a href=/>potenza</a></td>";
  html += "<td><a href=/ore>energia ore</a></td>";
  html += "<td><a href=/giorni>energia giorni</a></td>";
  html += "<td><a href=/setDateTime>set date time</a></td>";  
  html += "</tr></tbody></table>";
  html += "<h1>Energia Ora</h1>";
  html += "</body>";
  html += "<script>";
  html += "var webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');";
  html += "function sendButtonEvent(buttonId) {";
  html += "  webSocket.send(buttonId);";
  html += "}";
  html += getScript();
  html += "</script>";
  html += "</html>";
  return html;
}

// initializeSliders --> updateSlider --> updateBackgroundColor
String getScript() {
  String html = R"(

function riempiConsoleDebug(array1, array2) {
}

    )";
  return html;
}

String getHead() {
  String html = R"(
<title>Points</title>
<style>
</style>
  )";
  return html;
}