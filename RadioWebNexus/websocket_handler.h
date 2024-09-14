#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <WebSocketsServer_Generic.h>
#include <TimeLib.h>
#include <EEPROM.h>
#include "data_handling.h"
#include <LkBlinker.h>

extern WebSocketsServer webSocket;
extern int powerLimitValue;
extern LkBlinker blinker_segnalatore;
extern LkMultivibrator timer_flashDatetimeNotSet;  
extern const int LED_PIN; 
extern const bool LEDPIN_LOW;


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

        timer_flashDatetimeNotSet.stop();
        digitalWrite(LED_PIN,LEDPIN_LOW);

        //sendToMultiCatch("DATETIME-OK"); // invio comando a MultiCatch
      }
    } else if (message.equals("getHourEnergy")) {
        sendClient_hours(num); 
    } else if (message.equals("getDaysEnergy")){
        sendClient_days(num); 
    } else if (message.equals("getPowerLimit")){
        sendClient_powerLimit(num,powerLimitValue); 
        // Serial.println(powerLimitValue);
    } else if (message.startsWith("POWER-LIMIT=")){
        powerLimitValue = message.substring(12).toInt();
        EEPROM.write(0, powerLimitValue & 0xFF);
        EEPROM.write(0 + 1, (powerLimitValue >> 8) & 0xFF);
        EEPROM.commit(); 
    } else if (message.startsWith("ALARM-TEST")){
        blinker_segnalatore.enable(); 
    } else if (message.startsWith("SAVE")){
        saveData();
    } else {
      //Serial.println(message);
    }
  }
}

void setupWebSocket() {
    webSocket.begin();
    webSocket.onEvent(handleWebSocketMessage);
}

#endif // WEBSOCKET_HANDLER_H
