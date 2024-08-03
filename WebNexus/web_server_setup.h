#ifndef WEB_SERVER_SETUP_H
#define WEB_SERVER_SETUP_H

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

extern AsyncWebServer server;
bool ultimoDatoRadioAffidabile = true;
int potenza = 0;

void handleRoot(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
}


void setAffidabilitaDato(bool value){
  ultimoDatoRadioAffidabile=value;
}

void setPotenza (int value){
  potenza=value;
}

void setupWebServer() {
    server.serveStatic("/Chart.min.js", LittleFS, "/Chart.min.js");

    server.serveStatic("/config.js", LittleFS, "/config.js");

    // server.serveStatic("/campanella.mp3", LittleFS, "/campanella.mp3");

    server.on("/megane.html", HTTP_GET, [](AsyncWebServerRequest *request){
      String responseText = String(potenza) + "-" + String(ultimoDatoRadioAffidabile);
      
      // Invia la risposta
      request->send(200, "text/plain", responseText);
      Serial.println("AUTOmegane"); // invio comando a MultiCatch
    });

    server.on("/", HTTP_GET, handleRoot);

    server.on("/page1.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/page1.html", "text/html");
    });
    server.on("/page2.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/page2.html", "text/html");
    });
    server.on("/page3.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/page3.html", "text/html");
    });
    server.on("/page4.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/page4.html", "text/html");
    });
    server.on("/page5.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/page5.html", "text/html");
    });

    server.begin();
}

#endif // WEB_SERVER_SETUP_H
