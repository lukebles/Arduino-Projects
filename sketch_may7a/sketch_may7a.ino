#include <ESP8266WebServer.h>
#include <WebSocketsServer_Generic.h>
 
void setup(){
  Serial.begin(115200);
  WiFi.softAP("sid", "password1234");
  //WiFi.mode(WIFI_MODE_STA);
  Serial.println(WiFi.macAddress());
}
 
void loop(){

}