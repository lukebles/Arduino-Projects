#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

const char* ssid = "sid2";
const char* password = "pw12345678";
const char* url = "http://192.168.4.1/megane.html";

void setup() {
  Serial.begin(115200);
  delay(10);
  
  // Connect to Wi-Fi network
  Serial.println();
  Serial.println();
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

void loop() {
  if (WiFi.status() == WL_CONNECTED) { // Check WiFi connection status
    WiFiClient client;
    HTTPClient http;  // Declare an object of class HTTPClient
    
    http.begin(client, url);  // Specify request destination
    int httpCode = http.GET();  // Send the request
    
    if (httpCode > 0) { // Check the returning code
      String payload = http.getString();   // Get the request response payload
      
      Serial.println("HTTP Response:");
      Serial.println(payload);
      
    } else {
      Serial.println("Error on HTTP request");
    }
    
    http.end();   // Close connection
  } else {
    Serial.println("Error in WiFi connection");
  }
  
  delay(30000);    // Wait for 30 seconds
}

