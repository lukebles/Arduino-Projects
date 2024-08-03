#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LkMultivibrator.h>

const char* ssid = "sid2";
const char* password = "pw12345678";
const char* url = "http://192.168.4.1/megane.html";

const int pinLedWifi = 2;  // GPIO2
const int pinRele = 3;  // GPIO2
const unsigned long reconnectInterval = 8000; // 8 seconds
const unsigned long visitInterval = 8000; // 8 seconds
const unsigned long releOnDuration = 10000;// 1200000;  // 20 minutes in milliseconds
const unsigned long releOffDuration = 5000; //60000;   // 1 minute in milliseconds

LkMultivibrator reconnectTimer(reconnectInterval, MONOSTABLE);
LkMultivibrator visitSiteTimer(visitInterval, MONOSTABLE);
LkMultivibrator releOnTimer(releOnDuration, MONOSTABLE);
LkMultivibrator releOffTimer(releOffDuration, MONOSTABLE);

bool releSiPuoAccendere = true;

bool statoPrecedente = LOW;

void setup() {
  Serial.begin(115200);
  delay(10);
  
  pinMode(pinLedWifi, OUTPUT);
  digitalWrite(pinLedWifi, HIGH); // Turn LED off

  pinMode(pinRele, OUTPUT);
  digitalWrite(pinRele, LOW); // Turn RELE off

  // disattiva
  releOffTimer.stop();
  releOnTimer.stop();

  reconnectTimer.start();

  connectToWiFi();

}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(pinLedWifi, HIGH); // Turn LED off if not connected
    // tenta di ricollegarsi al wifi dopo
    // ogni 8 secondi
    if (reconnectTimer.expired()) {
      connectToWiFi();
      reconnectTimer.start();
    }
  } else {
    // il wifi è collegato
    digitalWrite(pinLedWifi, LOW); // accende il LED del wifi
    if (visitSiteTimer.expired()) {
      // ogni 8 secondi visita la pagina
      processWebData();
      visitSiteTimer.start();
    }
  }

}

void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void processWebData() {
  WiFiClient client;
  HTTPClient http;
  
  http.begin(client, url);
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("HTTP Response:");
    Serial.println(payload);
    handlePayload(payload);
  } else {
    Serial.println("Error on HTTP request");
  }
  
  http.end();
}

void handlePayload(String payload) {
  int separatorIndex = payload.indexOf('-');
  
  if (separatorIndex > 0) {
    String powerStr = payload.substring(0, separatorIndex);
    String validStr = payload.substring(separatorIndex + 1);
    
    int power = powerStr.toInt();
    int valid = validStr.toInt();

    if (digitalRead(pinRele)){
      // se il rele è acceso 
      if (power > 3600){
        // e c'è il superamento di energia
        // spegne il relè
        digitalWrite(pinRele, LOW); // Turn RELE off
      }
    }    


    if (power < 1100 && valid == 1) {
      digitalWrite(pinRele, HIGH); // Turn RELE on
    } else {
      digitalWrite(pinRele, LOW); // Turn RELE off
    }
  }
}
