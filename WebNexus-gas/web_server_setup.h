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

    // risposta per la pagina che consulta periodicamente
    // la presa di corrente a cui è collegata l'auto Megane
    server.on("/megane.html", HTTP_GET, [](AsyncWebServerRequest *request){
      // invia la stringa di risposta alla presa contenente la
      // potenza istantanea erogata attualmente dall'impianto
      // e se il dato che sta ricevendo è un dato valido
      // (in sostanza se il segnale radio proveniente dal contatore
      // dei lampeggi è recente)
      String responseText = String(potenza) + "-" + String(ultimoDatoRadioAffidabile);
      
      // Invia a multicatch sulla seriale un comando che
      // serve ad informare di disabilitare temporaneamente 
      // la campana
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
