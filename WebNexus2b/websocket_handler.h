#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <WebSocketsServer_Generic.h>
#include <TimeLib.h>
#include "data_handling.h"

extern WebSocketsServer webSocket;

void handleWebSocketMessage(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    String message = (char*)payload;
    // Serial.println(message);
    if (message.equals("getPowerData")) {
      sendClient_istant(num);
    } else if (message.startsWith("setTime:")) {
      String dateTimeString = message.substring(8);
      struct tm tm;
      if (sscanf(dateTimeString.c_str(), "%d-%d-%d %d:%d:%d",
                  &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                  &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
        tm.tm_isdst = 0;

        time_t t = mktime(&tm);
        setTime(t);

        webSocket.sendTXT(num, "Time updated successfully");
      }
    } else if (message.equals("getHourEnergy")) {
        sendClient_hours(num); 
    } else if (message.equals("getDaysEnergy")){
        sendClient_days(num); 
    } else {

    }
  }
}

void setupWebSocket() {
    webSocket.begin();
    webSocket.onEvent(handleWebSocketMessage);
}

#endif // WEBSOCKET_HANDLER_H
