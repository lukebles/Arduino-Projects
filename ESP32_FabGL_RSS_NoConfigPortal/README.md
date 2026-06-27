# ESP32 FabGL RSS - configurazione web senza config.h

File del progetto Arduino:

- `ESP32_FabGL_RSS_NoConfigPortal.ino`
- `LkWiFiConfigPortal.h`
- `LkWiFiConfigPortal.cpp`

Non serve più `config.h`.

## Avvio normale

L'ESP32 legge dalla memoria NVS le credenziali WiFi salvate e l'elenco RSS salvato.
Se non trova credenziali WiFi valide, apre automaticamente l'Access Point di configurazione.

## Modalità configurazione forzata

Tieni premuto il pulsante collegato a `BTN_NEXT_RSS` durante l'accensione.
Il dispositivo apre:

- SSID: `ESP32-RSS-SETUP`
- Pagina: `http://192.168.4.1/config.html`

Da lì puoi aprire:

- `/wifi.html` per cambiare solo le credenziali WiFi
- `/rss.html` per cambiare solo l'elenco RSS

## Librerie richieste

- FabGL 1.0.9
- Bounce2
- ESP32 Arduino core 2.0.4
