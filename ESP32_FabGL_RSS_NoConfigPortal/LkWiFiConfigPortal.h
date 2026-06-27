#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

struct LkRSSFeed {
  const char* label;
  const char* url;
};

/*
  LkWiFiConfigPortal

  Libreria semplice per ESP32 Arduino.

  Funzioni:
  - legge SSID/password salvati in NVS
  - prova a collegarsi al WiFi salvato
  - se non ci sono credenziali, oppure se forceConfigMode=true, apre un Access Point di configurazione
  - espone le pagine:
      http://IP_ESP32/config.html
      http://IP_ESP32/wifi.html
      http://IP_ESP32/rss.html
  - in modalità Access Point:
      SSID: ESP32-RSS-SETUP
      IP  : 192.168.4.1
      URL : http://192.168.4.1/config.html
             http://192.168.4.1/wifi.html
             http://192.168.4.1/rss.html

  Richiede nel loop():
      WiFiPortal.handleClient();
*/

class LkWiFiConfigPortal {
public:
  static const int MAX_RSS_FEEDS = 12;

  LkWiFiConfigPortal(uint16_t port = 80);

  void setRSSDefaults(const LkRSSFeed* defaults, int count);

  void begin(bool forceConfigMode = false,
             const char* apSSID = "ESP32-RSS-SETUP",
             const char* apPassword = "",
             uint32_t connectTimeoutMs = 20000);

  void handleClient();

  bool isConnected() const;
  bool isConfigAPActive() const;

  String ipString() const;
  String currentSSID() const;

  void clearSavedCredentials();
  void restartConnection();

  int rssFeedCount();
  String rssLabel(int index);
  String rssUrl(int index);
  uint32_t rssConfigVersion() const;
  void resetRSSFeedsToDefaults();

private:
  WebServer _server;
  Preferences _prefs;
  Preferences _rssPrefs;

  String _apSSID;
  String _apPassword;

  bool _apActive = false;
  bool _serverStarted = false;
  uint32_t _connectTimeoutMs = 20000;

  bool _rssEnabled = false;
  bool _rssPrefsOpen = false;
  int _defaultRSSCount = 0;
  String _defaultRSSLabels[MAX_RSS_FEEDS];
  String _defaultRSSUrls[MAX_RSS_FEEDS];
  uint32_t _rssVersion = 0;

  String _loadSSID();
  String _loadPassword();

  void _saveCredentials(const String& ssid, const String& password);

  bool _connectToWiFi(const String& ssid, const String& password);
  void _startConfigAP();
  void _startWebServer();

  void _handleRoot();
  void _handleConfigHtml();
  void _handleWifiHtml();
  void _handleWifiSave();
  void _handleWifiClear();

  void _handleRSSHtml();
  void _handleRSSSave();
  void _handleRSSDefaults();

  void _handleNotFound();

  String _htmlEscape(const String& s);
  String _buildConfigPage(const String& message = "");
  String _buildWifiPage(const String& message = "");
  String _buildRSSPage(const String& message = "");

  void _openRSSPrefsIfNeeded();
  int _loadRSSCount();
  void _saveRSSFeed(int index, const String& label, const String& url);
  void _clearRSSFeeds();
};
