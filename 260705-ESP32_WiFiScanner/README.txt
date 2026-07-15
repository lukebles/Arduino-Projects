ESP32_WiFiScanner
=================

Progetto Arduino IDE per ESP32:
- ESP32 in modalita' Access Point
- pagina web su http://192.168.4.1
- scansione WiFi 2.4 GHz per durata impostabile
- tabella ordinabile
- storico delle ultime 5 scansioni salvato in LittleFS
- massimo 30 access point salvati per scansione
- grafici a barre sotto alla tabella

File inclusi
------------
ESP32_WiFiScanner.ino
  Sketch Arduino completo.

data/index.html
  Interfaccia web con tabella ordinabile e grafici.

File da aggiungere
------------------
Per usare Chart.js come richiesto, scarica Chart.min.js versione 2.9.3 e copialo qui:

data/chart.min.js

Il progetto funziona anche senza chart.min.js: in quel caso la pagina usa un piccolo grafico Canvas interno di fallback.

Caricamento rapido
------------------
1. Apri la cartella ESP32_WiFiScanner con Arduino IDE.
2. Seleziona la tua scheda ESP32.
3. Installa/usa il tool di upload LittleFS per Arduino IDE 2.x oppure carica la cartella data/ con il metodo previsto dal tuo ambiente.
4. Carica prima il filesystem LittleFS con data/index.html e data/chart.min.js, se presente.
5. Carica lo sketch ESP32_WiFiScanner.ino.
6. Collegati alla rete WiFi ESP32_WIFI_SCANNER, password 12345678.
7. Apri http://192.168.4.1

Endpoint disponibili
--------------------
/                         pagina principale
/start?clientTime=...&duration=30
/status                   stato scansione
/last                     ultima scansione JSON
/history                  storico ultime 5 scansioni JSON
/clear-history            cancella storico

Note tecniche
-------------
- ESP32 classico scansiona il WiFi 2.4 GHz, non il 5 GHz.
- La larghezza canale e' indicata come n/d perche' WiFi.h non la espone sempre in modo portabile.
- Durante una scansione WiFi, la risposta web puo' essere meno fluida: e' normale.
- Per la data/ora, il browser invia l'orario locale all'ESP32 quando premi Avvia scansione.
