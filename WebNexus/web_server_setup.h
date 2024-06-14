#ifndef WEB_SERVER_SETUP_H
#define WEB_SERVER_SETUP_H

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

extern AsyncWebServer server;

void handleRoot(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
}

void setupWebServer() {
    server.serveStatic("/Chart.min.js", LittleFS, "/Chart.min.js");

    server.serveStatic("/config.js", LittleFS, "/config.js");

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
