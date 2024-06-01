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
    server.on("/", HTTP_GET, handleRoot);
    server.on("/page1.html", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/page1.html", "text/html");
    });

    server.begin();
}

#endif // WEB_SERVER_SETUP_H
