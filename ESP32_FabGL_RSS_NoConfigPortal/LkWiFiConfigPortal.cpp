#include "LkWiFiConfigPortal.h"

LkWiFiConfigPortal::LkWiFiConfigPortal(uint16_t port)
: _server(port)
{
}

void LkWiFiConfigPortal::setRSSDefaults(const LkRSSFeed* defaults, int count)
{
  _rssEnabled = true;

  if (count < 0)
    count = 0;

  if (count > MAX_RSS_FEEDS)
    count = MAX_RSS_FEEDS;

  _defaultRSSCount = count;

  for (int i = 0; i < MAX_RSS_FEEDS; ++i) {
    _defaultRSSLabels[i] = "";
    _defaultRSSUrls[i] = "";
  }

  for (int i = 0; i < count; ++i) {
    _defaultRSSLabels[i] = defaults[i].label ? defaults[i].label : "";
    _defaultRSSUrls[i] = defaults[i].url ? defaults[i].url : "";
  }
}

void LkWiFiConfigPortal::begin(bool forceConfigMode,
                               const char* apSSID,
                               const char* apPassword,
                               uint32_t connectTimeoutMs)
{
  _apSSID = apSSID ? apSSID : "ESP32-RSS-SETUP";
  _apPassword = apPassword ? apPassword : "";
  _connectTimeoutMs = connectTimeoutMs;

  _prefs.begin("wifi_cfg", false);

  if (_rssEnabled) {
    _openRSSPrefsIfNeeded();
  }

  String ssid = _loadSSID();
  String pass = _loadPassword();

  bool ok = false;

  WiFi.setSleep(false);

  if (!forceConfigMode && ssid.length() > 0) {
    WiFi.mode(WIFI_STA);
    ok = _connectToWiFi(ssid, pass);
  }

  if (forceConfigMode || !ok) {
    _startConfigAP();
  }

  _startWebServer();
}

void LkWiFiConfigPortal::handleClient()
{
  _server.handleClient();
}

bool LkWiFiConfigPortal::isConnected() const
{
  return WiFi.status() == WL_CONNECTED;
}

bool LkWiFiConfigPortal::isConfigAPActive() const
{
  return _apActive;
}

String LkWiFiConfigPortal::ipString() const
{
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }

  if (_apActive) {
    return WiFi.softAPIP().toString();
  }

  return "";
}

String LkWiFiConfigPortal::currentSSID() const
{
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.SSID();
  }

  return "";
}

void LkWiFiConfigPortal::clearSavedCredentials()
{
  _prefs.remove("ssid");
  _prefs.remove("pass");
}

void LkWiFiConfigPortal::restartConnection()
{
  String ssid = _loadSSID();
  String pass = _loadPassword();

  WiFi.disconnect(false, false);
  delay(300);

  bool ok = false;

  if (ssid.length() > 0) {
    ok = _connectToWiFi(ssid, pass);
  }

  if (!ok && !_apActive) {
    _startConfigAP();
  }
}

String LkWiFiConfigPortal::_loadSSID()
{
  return _prefs.getString("ssid", "");
}

String LkWiFiConfigPortal::_loadPassword()
{
  return _prefs.getString("pass", "");
}

void LkWiFiConfigPortal::_saveCredentials(const String& ssid, const String& password)
{
  _prefs.putString("ssid", ssid);
  _prefs.putString("pass", password);
}

bool LkWiFiConfigPortal::_connectToWiFi(const String& ssid, const String& password)
{
  // Se l'Access Point di configurazione è già attivo, lo manteniamo acceso
  // mentre proviamo a collegarci alla rete esterna.
  WiFi.mode(_apActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  uint32_t start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < _connectTimeoutMs) {
    delay(250);
  }

  return WiFi.status() == WL_CONNECTED;
}

void LkWiFiConfigPortal::_startConfigAP()
{
  _apActive = true;

  if (_apPassword.length() >= 8) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(_apSSID.c_str(), _apPassword.c_str());
  } else {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(_apSSID.c_str());
  }
}

void LkWiFiConfigPortal::_startWebServer()
{
  if (_serverStarted)
    return;

  _server.on("/", HTTP_GET, [this]() {
    _handleRoot();
  });

  _server.on("/config.html", HTTP_GET, [this]() {
    _handleConfigHtml();
  });

  _server.on("/wifi.html", HTTP_GET, [this]() {
    _handleWifiHtml();
  });

  _server.on("/wifi-save", HTTP_POST, [this]() {
    _handleWifiSave();
  });

  _server.on("/wifi-clear", HTTP_POST, [this]() {
    _handleWifiClear();
  });

  _server.on("/rss.html", HTTP_GET, [this]() {
    _handleRSSHtml();
  });

  _server.on("/rss-save", HTTP_POST, [this]() {
    _handleRSSSave();
  });

  _server.on("/rss-defaults", HTTP_POST, [this]() {
    _handleRSSDefaults();
  });

  _server.onNotFound([this]() {
    _handleNotFound();
  });

  _server.begin();
  _serverStarted = true;
}

void LkWiFiConfigPortal::_handleRoot()
{
  _server.sendHeader("Location", "/config.html", true);
  _server.send(302, "text/plain", "");
}

void LkWiFiConfigPortal::_handleConfigHtml()
{
  _server.send(200, "text/html; charset=utf-8", _buildConfigPage());
}

void LkWiFiConfigPortal::_handleWifiHtml()
{
  _server.send(200, "text/html; charset=utf-8", _buildWifiPage());
}

void LkWiFiConfigPortal::_handleWifiSave()
{
  String ssid = _server.arg("ssid");
  String pass = _server.arg("pass");

  ssid.trim();

  if (ssid.length() == 0) {
    _server.send(400, "text/html; charset=utf-8",
                 _buildWifiPage("Errore: SSID vuoto."));
    return;
  }

  _saveCredentials(ssid, pass);

  WiFi.disconnect(false, false);
  delay(300);

  bool ok = _connectToWiFi(ssid, pass);

  String msg;

  if (ok) {
    msg = "Credenziali salvate. Connessione riuscita. IP: " + WiFi.localIP().toString();
  } else {
    msg = "Credenziali salvate, ma connessione non riuscita. "
          "Resto disponibile tramite Access Point di configurazione.";

    if (!_apActive) {
      _startConfigAP();
    }
  }

  _server.send(200, "text/html; charset=utf-8", _buildWifiPage(msg));
}

void LkWiFiConfigPortal::_handleWifiClear()
{
  clearSavedCredentials();

  _server.send(200, "text/html; charset=utf-8",
               _buildWifiPage("Credenziali salvate cancellate. Riavvia l'ESP32 oppure inserisci nuove credenziali."));
}

void LkWiFiConfigPortal::_handleRSSHtml()
{
  _server.send(200, "text/html; charset=utf-8", _buildRSSPage());
}

void LkWiFiConfigPortal::_handleRSSSave()
{
  if (!_rssEnabled) {
    _server.send(404, "text/plain; charset=utf-8", "Configurazione RSS non abilitata.");
    return;
  }

  _openRSSPrefsIfNeeded();

  String labels[MAX_RSS_FEEDS];
  String urls[MAX_RSS_FEEDS];
  int count = 0;

  for (int i = 0; i < MAX_RSS_FEEDS; ++i) {
    String labelName = "label" + String(i);
    String urlName = "url" + String(i);

    String label = _server.arg(labelName);
    String url = _server.arg(urlName);

    label.trim();
    url.trim();

    if (label.length() == 0 && url.length() == 0) {
      continue;
    }

    if (url.length() == 0) {
      _server.send(400, "text/html; charset=utf-8",
                   _buildRSSPage("Errore: una riga ha il nome ma non l'URL."));
      return;
    }

    if (!url.startsWith("http://") && !url.startsWith("https://")) {
      _server.send(400, "text/html; charset=utf-8",
                   _buildRSSPage("Errore: gli URL devono iniziare con http:// oppure https://"));
      return;
    }

    if (label.length() == 0) {
      label = "RSS " + String(count + 1);
    }

    labels[count] = label;
    urls[count] = url;
    count++;
  }

  if (count == 0) {
    _server.send(400, "text/html; charset=utf-8",
                 _buildRSSPage("Errore: devi lasciare almeno un feed RSS."));
    return;
  }

  _clearRSSFeeds();
  _rssPrefs.putInt("count", count);

  for (int i = 0; i < count; ++i) {
    _saveRSSFeed(i, labels[i], urls[i]);
  }

  _rssVersion++;

  _server.send(200, "text/html; charset=utf-8",
               _buildRSSPage("Elenco RSS salvato. Il lettore ricaricherà il feed selezionato."));
}

void LkWiFiConfigPortal::_handleRSSDefaults()
{
  resetRSSFeedsToDefaults();

  _server.send(200, "text/html; charset=utf-8",
               _buildRSSPage("Elenco RSS riportato ai valori di default."));
}

void LkWiFiConfigPortal::_handleNotFound()
{
  _server.send(404, "text/plain; charset=utf-8", "Pagina non trovata. Usa /config.html, /wifi.html oppure /rss.html");
}

String LkWiFiConfigPortal::_htmlEscape(const String& s)
{
  String out;
  out.reserve(s.length() + 16);

  for (int i = 0; i < s.length(); ++i) {
    char c = s[i];

    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c; break;
    }
  }

  return out;
}

String LkWiFiConfigPortal::_buildConfigPage(const String& message)
{
  String msg = "";

  if (message.length() > 0) {
    msg = "<div class='msg'>" + _htmlEscape(message) + "</div>";
  }

  String html;
  html.reserve(4200);

  html += "<!doctype html><html lang='it'><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configurazione ESP32 RSS</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:0;padding:20px;}";
  html += ".box{max-width:700px;margin:auto;background:#1d1d1d;border:1px solid #444;border-radius:12px;padding:18px;}";
  html += "h1{font-size:24px;margin-top:0;}";
  html += "a.btn{display:block;background:#2b7cff;color:#fff;text-decoration:none;padding:14px;border-radius:10px;margin:12px 0;font-size:18px;}";
  html += "a{color:#8ab4ff;}";
  html += ".msg{background:#063;border:1px solid #0a5;padding:10px;border-radius:8px;margin-bottom:14px;}";
  html += ".info{background:#000;border:1px solid #333;padding:10px;border-radius:8px;margin-bottom:14px;line-height:1.5;}";
  html += "small{color:#bbb;}";
  html += "</style>";
  html += "</head><body><div class='box'>";

  html += "<h1>Configurazione ESP32 RSS</h1>";
  html += msg;

  html += "<div class='info'>";
  html += "<b>Connesso WiFi:</b> " + String(isConnected() ? "SI" : "NO") + "<br>";
  html += "<b>SSID attuale:</b> " + _htmlEscape(currentSSID()) + "<br>";
  html += "<b>IP:</b> " + _htmlEscape(ipString()) + "<br>";
  if (_apActive) {
    html += "<b>Access Point:</b> " + _htmlEscape(_apSSID) + "<br>";
    html += "<b>IP AP:</b> " + WiFi.softAPIP().toString() + "<br>";
  }
  html += "<b>Feed RSS configurati:</b> " + String(rssFeedCount()) + "<br>";
  html += "</div>";

  html += "<a class='btn' href='/wifi.html'>Configura credenziali WiFi</a>";
  html += "<a class='btn' href='/rss.html'>Configura elenco RSS</a>";

  html += "<p><small>Puoi modificare solo il WiFi, solo l'elenco RSS, oppure entrambi. "
          "Per forzare questa modalità all'accensione, tieni premuto il pulsante cambio RSS durante il boot.</small></p>";

  html += "</div></body></html>";

  return html;
}

String LkWiFiConfigPortal::_buildWifiPage(const String& message)
{
  String savedSSID = _loadSSID();

  String connected = isConnected() ? "SI" : "NO";
  String ip = ipString();
  String ssidNow = currentSSID();

  String apInfo = "";

  if (_apActive) {
    apInfo = "<p><b>Access Point configurazione attivo:</b><br>"
             "SSID: " + _htmlEscape(_apSSID) + "<br>"
             "IP: " + WiFi.softAPIP().toString() + "</p>";
  }

  String msg = "";

  if (message.length() > 0) {
    msg = "<div class='msg'>" + _htmlEscape(message) + "</div>";
  }

  String html;
  html.reserve(5200);

  html += "<!doctype html><html lang='it'><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configurazione WiFi ESP32</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:0;padding:20px;}";
  html += ".box{max-width:620px;margin:auto;background:#1d1d1d;border:1px solid #444;border-radius:12px;padding:18px;}";
  html += "h1{font-size:24px;margin-top:0;}";
  html += "a{color:#8ab4ff;}";
  html += "label{display:block;margin-top:14px;font-weight:bold;}";
  html += "input{width:100%;box-sizing:border-box;font-size:18px;padding:10px;margin-top:5px;border-radius:8px;border:1px solid #666;background:#000;color:#fff;}";
  html += "button{font-size:18px;padding:10px 14px;margin-top:18px;border:0;border-radius:8px;background:#2b7cff;color:white;}";
  html += "button.danger{background:#b3261e;}";
  html += ".msg{background:#063;border:1px solid #0a5;padding:10px;border-radius:8px;margin-bottom:14px;}";
  html += ".info{background:#000;border:1px solid #333;padding:10px;border-radius:8px;margin-bottom:14px;line-height:1.5;}";
  html += "small{color:#bbb;}";
  html += "</style>";
  html += "</head><body><div class='box'>";

  html += "<h1>Configurazione WiFi ESP32</h1>";
  html += "<p><a href='/config.html'>Menu configurazione</a> | <a href='/rss.html'>Configura RSS</a></p>";
  html += msg;

  html += "<div class='info'>";
  html += "<b>Connesso:</b> " + connected + "<br>";
  html += "<b>SSID attuale:</b> " + _htmlEscape(ssidNow) + "<br>";
  html += "<b>IP:</b> " + _htmlEscape(ip) + "<br>";
  html += "<b>SSID salvato:</b> " + _htmlEscape(savedSSID) + "<br>";
  html += "</div>";

  html += apInfo;

  html += "<form method='POST' action='/wifi-save'>";
  html += "<label for='ssid'>SSID WiFi</label>";
  html += "<input id='ssid' name='ssid' value='" + _htmlEscape(savedSSID) + "' autocomplete='off'>";

  html += "<label for='pass'>Password WiFi</label>";
  html += "<input id='pass' name='pass' type='password' value=''>";
  html += "<small>Lascia vuoto solo se la rete non ha password. Per cambiare rete protetta, reinserisci la password.</small>";

  html += "<button type='submit'>Salva e collega</button>";
  html += "</form>";

  html += "<form method='POST' action='/wifi-clear'>";
  html += "<button class='danger' type='submit'>Cancella credenziali salvate</button>";
  html += "</form>";

  html += "<p><small>Pagina disponibile da rete locale su <b>/wifi.html</b>. "
          "Se il WiFi non funziona, collegati all'Access Point di configurazione e apri <b>http://192.168.4.1/wifi.html</b>.</small></p>";

  html += "</div></body></html>";

  return html;
}

void LkWiFiConfigPortal::_openRSSPrefsIfNeeded()
{
  if (!_rssPrefsOpen) {
    _rssPrefs.begin("rss_cfg", false);
    _rssPrefsOpen = true;
  }
}

int LkWiFiConfigPortal::_loadRSSCount()
{
  if (!_rssEnabled)
    return 0;

  _openRSSPrefsIfNeeded();

  int count = _rssPrefs.getInt("count", -1);

  if (count < 0) {
    return _defaultRSSCount;
  }

  if (count > MAX_RSS_FEEDS)
    count = MAX_RSS_FEEDS;

  return count;
}

int LkWiFiConfigPortal::rssFeedCount()
{
  return _loadRSSCount();
}

String LkWiFiConfigPortal::rssLabel(int index)
{
  if (!_rssEnabled)
    return "";

  int count = _loadRSSCount();

  if (index < 0 || index >= count)
    return "";

  _openRSSPrefsIfNeeded();

  String key = "l" + String(index);
  String saved = _rssPrefs.getString(key.c_str(), "");

  if (saved.length() > 0)
    return saved;

  if (index < _defaultRSSCount)
    return _defaultRSSLabels[index];

  return "";
}

String LkWiFiConfigPortal::rssUrl(int index)
{
  if (!_rssEnabled)
    return "";

  int count = _loadRSSCount();

  if (index < 0 || index >= count)
    return "";

  _openRSSPrefsIfNeeded();

  String key = "u" + String(index);
  String saved = _rssPrefs.getString(key.c_str(), "");

  if (saved.length() > 0)
    return saved;

  if (index < _defaultRSSCount)
    return _defaultRSSUrls[index];

  return "";
}

uint32_t LkWiFiConfigPortal::rssConfigVersion() const
{
  return _rssVersion;
}

void LkWiFiConfigPortal::_saveRSSFeed(int index, const String& label, const String& url)
{
  _openRSSPrefsIfNeeded();

  String keyL = "l" + String(index);
  String keyU = "u" + String(index);

  _rssPrefs.putString(keyL.c_str(), label);
  _rssPrefs.putString(keyU.c_str(), url);
}

void LkWiFiConfigPortal::_clearRSSFeeds()
{
  _openRSSPrefsIfNeeded();

  for (int i = 0; i < MAX_RSS_FEEDS; ++i) {
    String keyL = "l" + String(i);
    String keyU = "u" + String(i);
    _rssPrefs.remove(keyL.c_str());
    _rssPrefs.remove(keyU.c_str());
  }

  _rssPrefs.remove("count");
}

void LkWiFiConfigPortal::resetRSSFeedsToDefaults()
{
  if (!_rssEnabled)
    return;

  _clearRSSFeeds();
  _rssPrefs.putInt("count", _defaultRSSCount);

  for (int i = 0; i < _defaultRSSCount; ++i) {
    _saveRSSFeed(i, _defaultRSSLabels[i], _defaultRSSUrls[i]);
  }

  _rssVersion++;
}

String LkWiFiConfigPortal::_buildRSSPage(const String& message)
{
  String msg = "";

  if (message.length() > 0) {
    msg = "<div class='msg'>" + _htmlEscape(message) + "</div>";
  }

  String html;
  html.reserve(12000);

  html += "<!doctype html><html lang='it'><head>";
  html += "<meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configurazione RSS ESP32</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;background:#111;color:#eee;margin:0;padding:20px;}";
  html += ".box{max-width:920px;margin:auto;background:#1d1d1d;border:1px solid #444;border-radius:12px;padding:18px;}";
  html += "h1{font-size:24px;margin-top:0;}";
  html += "a{color:#8ab4ff;}";
  html += "label{font-weight:bold;}";
  html += ".row{border:1px solid #333;background:#000;border-radius:10px;padding:12px;margin:12px 0;}";
  html += "input{width:100%;box-sizing:border-box;font-size:16px;padding:9px;margin:5px 0 10px 0;border-radius:8px;border:1px solid #666;background:#050505;color:#fff;}";
  html += "button{font-size:17px;padding:10px 14px;margin-top:12px;margin-right:8px;border:0;border-radius:8px;background:#2b7cff;color:white;}";
  html += "button.danger{background:#b3261e;}";
  html += ".msg{background:#063;border:1px solid #0a5;padding:10px;border-radius:8px;margin-bottom:14px;}";
  html += ".info{background:#000;border:1px solid #333;padding:10px;border-radius:8px;margin-bottom:14px;line-height:1.5;}";
  html += "small{color:#bbb;}";
  html += "</style>";
  html += "</head><body><div class='box'>";

  html += "<h1>Configurazione feed RSS</h1>";
  html += "<p><a href='/config.html'>Menu configurazione</a> | <a href='/wifi.html'>Configura WiFi</a></p>";
  html += msg;

  html += "<div class='info'>";
  html += "<b>Feed configurati:</b> " + String(rssFeedCount()) + "<br>";
  html += "<b>Massimo feed:</b> " + String(MAX_RSS_FEEDS) + "<br>";
  html += "<b>IP pagina:</b> " + _htmlEscape(ipString()) + "<br>";
  html += "</div>";

  html += "<form method='POST' action='/rss-save'>";

  for (int i = 0; i < MAX_RSS_FEEDS; ++i) {
    String label = rssLabel(i);
    String url = rssUrl(i);

    html += "<div class='row'>";
    html += "<h3>Feed " + String(i) + "</h3>";
    html += "<label for='label" + String(i) + "'>Nome visualizzato</label>";
    html += "<input id='label" + String(i) + "' name='label" + String(i) + "' value='" + _htmlEscape(label) + "' autocomplete='off'>";
    html += "<label for='url" + String(i) + "'>URL RSS</label>";
    html += "<input id='url" + String(i) + "' name='url" + String(i) + "' value='" + _htmlEscape(url) + "' autocomplete='off'>";
    html += "<small>Lascia vuoti nome e URL per eliminare questa riga.</small>";
    html += "</div>";
  }

  html += "<button type='submit'>Salva elenco RSS</button>";
  html += "</form>";

  html += "<form method='POST' action='/rss-defaults'>";
  html += "<button class='danger' type='submit'>Ripristina feed di default</button>";
  html += "</form>";

  html += "<p><small>Dopo il salvataggio il lettore ricarica il canale selezionato da <b>STAZIONE_RSS</b>. "
          "Gli indici partono da 0. Se una riga viene eliminata, gli indici successivi si ricompattano.</small></p>";

  html += "</div></body></html>";

  return html;
}
