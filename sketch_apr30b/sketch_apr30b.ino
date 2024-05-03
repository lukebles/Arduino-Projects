#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
ESP8266WebServer server(80);

// Includi qui il file Chart.js come una stringa C++
const char* chartJs = "/* Qui va il tuo JavaScript minificato di Chart.js */";

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><script>";
  html += chartJs;
  html += "</script></head><body><canvas id='barChart' width='400' height='400'></canvas><script>var ctx = document.getElementById('barChart').getContext('2d');var myChart = new Chart(ctx, {type: 'bar',data: {labels: ['Red', 'Blue', 'Yellow', 'Green', 'Purple', 'Orange'],datasets: [{label: '# of Votes',data: [12, 19, 3, 5, 2, 3],backgroundColor: ['rgba(255, 99, 132, 0.2)','rgba(54, 162, 235, 0.2)','rgba(255, 206, 86, 0.2)','rgba(75, 192, 192, 0.2)','rgba(153, 102, 255, 0.2)','rgba(255, 159, 64, 0.2)'],borderColor: ['rgba(255, 99, 132, 1)','rgba(54, 162, 235, 1)','rgba(255, 206, 86, 1)','rgba(75, 192, 192, 1)','rgba(153, 102, 255, 1)','rgba(255, 159, 64, 1)'],borderWidth: 1}]},options: {scales: {y: {beginAtZero: true}}}});</script></body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();
}
