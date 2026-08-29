# ESP32 Meteo – ESP-NOW, Access Point, WebSocket e LittleFS

Questo progetto trasforma un ESP32 in una centralina meteo locale capace di:

- ricevere temperatura, umidità e pressione tramite ESP-NOW;
- creare una propria rete Wi-Fi, senza collegarsi a un router;
- mostrare i dati in una pagina web;
- aggiornare automaticamente la pagina tramite WebSocket quando arriva una nuova misura;
- visualizzare le ultime 24 ore con dettaglio di 15 minuti;
- visualizzare le medie giornaliere degli ultimi 30 giorni;
- salvare pagina web, Chart.js e storico su LittleFS;
- ricevere data, ora e fuso orario direttamente dal browser.

---

## Funzionamento generale

La centrale utilizza contemporaneamente:

```cpp
WiFi.mode(WIFI_AP_STA);
```

Le due interfacce hanno funzioni differenti:

- **STA** viene mantenuta attiva per ESP-NOW e conserva il MAC utilizzato dal vecchio ricevitore;
- **AP** crea la rete Wi-Fi locale alla quale collegare computer, tablet o telefono.

La centrale non si collega a un router perché nel programma non viene mai eseguito:

```cpp
WiFi.begin(...);
```

La rete creata dall'ESP32 è:

```text
SSID: ESP32-Meteo
Password: meteo1234
Pagina web: http://192.168.4.1/
WebSocket: ws://192.168.4.1:81/
Canale Wi-Fi/ESP-NOW: 13
```

---

## Struttura del progetto

La cartella deve essere organizzata così:

```text
260715_centraleESP32_espnow_webpage/
├── 260715_centraleESP32_espnow_webpage.ino
├── README.md
└── data/
    ├── index.html
    └── chart.min.js
```

Il file `chart.min.js` deve essere la versione:

```text
Chart.js 2.9.3
```

Questa versione è stata scelta per migliorare la compatibilità con browser meno recenti, indicativamente del periodo 2012-2013.

---

## Librerie necessarie

Le librerie seguenti fanno parte del core ESP32:

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
```

Deve invece essere installata separatamente la libreria:

```text
WebSockets
Autore: Markus Sattler
```

Da Arduino IDE:

```text
Strumenti
→ Gestione librerie
→ cerca "WebSockets"
→ installa la libreria di Markus Sattler
```

Nel programma viene utilizzata tramite:

```cpp
#include <WebSocketsServer.h>
```

---

## Formato del pacchetto ESP-NOW

Il trasmettitore deve inviare una struttura compatibile con:

```cpp
struct __attribute__((packed)) PacketEnvironment {
  uint8_t nodeId;
  uint8_t payloadType;
  uint32_t seq;
  int16_t temp_x100;
  int16_t hum_x100;
  uint32_t press_x100;
  uint8_t sensorType;
};
```

Significato dei campi:

```text
nodeId         identificativo del nodo trasmettitore
payloadType    tipo del pacchetto
seq            numero progressivo della trasmissione
temp_x100      temperatura in °C moltiplicata per 100
hum_x100       umidità relativa moltiplicata per 100
press_x100     pressione in hPa moltiplicata per 100
sensorType     tipo di sensore utilizzato
```

Esempio:

```text
temperatura reale: 23,47 °C
temp_x100: 2347

umidità reale: 51,32 %
hum_x100: 5132

pressione reale: 1008,75 hPa
press_x100: 100875
```

Per un BMP280, che non misura l'umidità:

```cpp
hum_x100 = -1;
```

---

## Canale ESP-NOW

La centrale utilizza:

```cpp
#define ESPNOW_CHANNEL 13
```

Anche il trasmettitore deve lavorare sul canale 13.

Nel monitor seriale viene mostrato il canale effettivamente utilizzato:

```text
Canale Wi-Fi/ESP-NOW effettivo: 13
```

Se il trasmettitore e la centrale utilizzano canali differenti, i pacchetti non vengono ricevuti.

---

## MAC ESP-NOW

La modalità `WIFI_AP_STA` mantiene attiva l'interfaccia Station utilizzata dal precedente programma ESP-NOW.

Nel monitor seriale vengono stampati entrambi i MAC:

```text
MAC STA usato da ESP-NOW: XX:XX:XX:XX:XX:XX
MAC AP della pagina web: XX:XX:XX:XX:XX:XX
```

Se il trasmettitore invia in unicast, il destinatario deve essere il:

```text
MAC STA usato da ESP-NOW
```

Il MAC AP viene invece utilizzato dalla rete Wi-Fi creata dalla centrale.

Se il trasmettitore utilizza il broadcast:

```cpp
FF:FF:FF:FF:FF:FF
```

non è necessario configurare il MAC specifico della centrale.

---

## Data e ora ricevute dal browser

La centrale non utilizza:

- router;
- Internet;
- server NTP;
- RTC esterno.

Quando viene aperta la pagina web, JavaScript invia all'ESP32:

- Unix epoch UTC;
- differenza locale rispetto a UTC;
- fuso orario corrente del browser.

La richiesta viene inviata all'indirizzo:

```text
POST /api/time
```

Dopo la sincronizzazione, l'ESP32 mantiene il tempo utilizzando:

```cpp
esp_timer_get_time()
```

La pagina ripete la sincronizzazione ogni 5 minuti per correggere:

- deriva dell'orologio;
- variazioni dell'ora legale;
- eventuali correzioni dell'orologio del dispositivo.

### Importante

Dopo ogni riavvio della centrale è necessario aprire almeno una volta la pagina:

```text
http://192.168.4.1/
```

Le misure ricevute prima della prima sincronizzazione:

- vengono mostrate come ultima misura ricevuta;
- non vengono inserite nello storico, perché non possono essere associate a una data e ora affidabili.

---

## Grafico delle ultime 24 ore

Il primo grafico contiene:

```text
96 intervalli
```

Ogni intervallo dura:

```text
15 minuti
```

Calcolo:

```text
24 ore × 4 intervalli ogni ora = 96 punti
```

Dato che il sensore trasmette indicativamente ogni 5 minuti, ogni intervallo da 15 minuti contiene normalmente circa tre misure.

Il valore mostrato è la media delle misure ricevute nel relativo intervallo.

Esempio:

```text
10:00 → 22,10 °C
10:05 → 22,30 °C
10:10 → 22,50 °C
```

Valore visualizzato per l'intervallo `10:00–10:14`:

```text
(22,10 + 22,30 + 22,50) / 3 = 22,30 °C
```

L'intervallo corrente viene mostrato anche se non è ancora concluso. In questo caso il punto rappresenta una media parziale.

Le etichette hanno il formato:

```text
gg/mm HH:MM
```

---

## Grafico degli ultimi 30 giorni

Il secondo grafico contiene:

```text
30 intervalli giornalieri
```

Ogni punto rappresenta la media di tutte le misure ricevute nel relativo giorno.

Il giorno corrente viene mostrato come media parziale.

Le etichette hanno il formato:

```text
gg/mm
```

---

## Grandezze visualizzate

Entrambi i grafici mostrano:

- temperatura in °C;
- umidità relativa in %;
- pressione atmosferica in hPa.

Le tre grandezze utilizzano assi verticali differenti perché hanno scale numeriche molto diverse.

Nel caso di un BMP280, i valori di umidità vengono restituiti come `null` e Chart.js mostra un'interruzione della relativa linea.

---

## Aggiornamento tramite WebSocket

Il server WebSocket utilizza la porta:

```text
81
```

Il browser si collega a:

```text
ws://192.168.4.1:81/
```

Quando arriva una misura ESP-NOW, la centrale esegue:

```cpp
webSocket.broadcastTXT("measurement");
```

Il browser riceve il messaggio e richiede immediatamente:

```text
GET /api/history
```

La sequenza completa è:

```text
il trasmettitore invia il pacchetto
        ↓
la callback ESP-NOW riceve il pacchetto
        ↓
la misura viene elaborata nel loop principale
        ↓
viene aggiornata la media dei 15 minuti
        ↓
viene aggiornata la media giornaliera
        ↓
il WebSocket invia "measurement"
        ↓
il browser richiede /api/history
        ↓
valori e grafici vengono aggiornati
```

Il WebSocket viene utilizzato soltanto come notifica.

Il JSON completo non viene inviato direttamente tramite WebSocket: il browser lo richiede al server HTTP. Questo mantiene più semplice la gestione dei dati e riduce il lavoro svolto nella callback ESP-NOW.

Se il collegamento WebSocket si interrompe, il browser tenta automaticamente una nuova connessione dopo 3 secondi.

---

## Server HTTP

Il server HTTP utilizza la porta:

```text
80
```

Risorse disponibili:

```text
/                 pagina principale
/index.html       pagina principale
/chart.min.js     libreria Chart.js
/api/time         ricezione di data e ora dal browser
/api/history      dati JSON per grafici e valori correnti
```

---

## Compatibilità con browser meno recenti

La pagina è scritta evitando diverse funzionalità JavaScript moderne.

Non vengono utilizzati:

- `fetch`;
- Promise;
- funzioni freccia;
- `let`;
- `const`;
- `async` e `await`;
- moduli JavaScript.

Vengono utilizzati:

- `XMLHttpRequest`;
- sintassi JavaScript ES5;
- WebSocket standard;
- Chart.js 2.9.3.

La compatibilità WebSocket dipende comunque dal browser specifico.

---

## Salvataggio su LittleFS

LittleFS contiene:

```text
/index.html
/chart.min.js
/history.bin
```

Lo storico viene salvato nel file:

```text
/history.bin
```

Il salvataggio avviene:

- alla chiusura di ogni intervallo da 15 minuti;
- alla chiusura del giorno;
- periodicamente durante l'intervallo corrente.

Per ridurre il rischio di corruzione, il programma scrive prima:

```text
/history.tmp
```

e successivamente lo rinomina in:

```text
/history.bin
```

---

## Cambio del formato dello storico

La versione precedente memorizzava 24 intervalli orari.

La nuova versione memorizza:

```text
96 intervalli da 15 minuti
```

Per questo motivo la versione della struttura persistente è stata incrementata.

Se LittleFS contiene uno storico del vecchio formato, il programma mostra:

```text
Storico di formato precedente: verrà reinizializzato
```

e crea automaticamente un nuovo archivio.

---

## Caricamento di LittleFS

Prima di caricare lo sketch, verifica che la cartella `data` contenga:

```text
index.html
chart.min.js
```

### Arduino IDE

Serve il comando o plugin compatibile con la versione del core ESP32 utilizzata per caricare la partizione LittleFS.

Carica:

1. il contenuto della cartella `data`;
2. lo sketch `.ino`.

### PlatformIO

Configurazione tipica:

```ini
board_build.filesystem = littlefs
```

Comandi:

```bash
pio run -t uploadfs
pio run -t upload
```

---

## Prima accensione

1. Aprire il monitor seriale a:

```text
115200 baud
```

2. Riavviare l'ESP32.

3. Controllare che compaiano messaggi simili a:

```text
MAC STA usato da ESP-NOW: XX:XX:XX:XX:XX:XX
MAC AP della pagina web: XX:XX:XX:XX:XX:XX
SSID access point: ESP32-Meteo
IP pagina web: http://192.168.4.1
Canale Wi-Fi/ESP-NOW effettivo: 13
Server HTTP avviato sulla porta 80
Server WebSocket avviato sulla porta 81
Centrale pronta
```

4. Collegare il computer o il tablet alla rete:

```text
ESP32-Meteo
```

5. Inserire la password:

```text
meteo1234
```

6. Aprire:

```text
http://192.168.4.1/
```

7. Verificare nella pagina:

```text
Data e ora inviate correttamente all'ESP32
WebSocket collegato
```

---

## Diagnostica ESP-NOW

Ogni 10 secondi il monitor seriale mostra:

```text
Callback ESP-NOW: 12 | ultima lunghezza: 15 | lunghezza attesa: 15
```

Interpretazione:

### Callback ESP-NOW sempre a zero

```text
Callback ESP-NOW: 0
```

Possibili cause:

- trasmettitore sul canale sbagliato;
- MAC destinatario errato;
- trasmettitore non attivo;
- peer ESP-NOW configurato in modo errato;
- distanza eccessiva;
- antenna non collegata, se richiesta dalla scheda.

### Callback aumenta, ma la misura non viene elaborata

Possibili cause:

- lunghezza del pacchetto differente;
- `nodeId` differente;
- `payloadType` differente;
- struttura del pacchetto non identica tra trasmettitore e ricevitore.

Valori attesi:

```cpp
#define NODE_TX1 1
#define TYPE_ENVIRONMENT 10
```

### La pagina funziona ma non si aggiorna automaticamente

Controllare che mostri:

```text
WebSocket collegato
```

Se mostra un errore:

- verificare che la libreria WebSockets sia installata;
- verificare che il server WebSocket sia stato avviato;
- verificare che il browser supporti WebSocket;
- verificare che la pagina sia aperta da `http://192.168.4.1/`;
- evitare di aprire direttamente il file `index.html` dal disco.

---

## Modifica delle credenziali Wi-Fi

Nel programma:

```cpp
const char *AP_SSID = "ESP32-Meteo";
const char *AP_PASSWORD = "meteo1234";
```

La password WPA2 deve contenere almeno 8 caratteri.

---

## Sicurezza della callback ESP-NOW

La callback ESP-NOW non:

- scrive su LittleFS;
- genera il JSON;
- aggiorna direttamente il WebSocket;
- esegue elaborazioni lunghe.

La callback copia la misura in una coda FreeRTOS:

```cpp
xQueueSend(...)
```

La misura viene successivamente elaborata nel `loop()` principale.

Questo riduce il rischio di blocchi o problemi nel task Wi-Fi.

---

## Note finali

- La centrale non richiede Internet.
- Il router non viene utilizzato.
- Data e ora vengono fornite dal browser.
- ESP-NOW e access point condividono il canale 13.
- Il browser si aggiorna immediatamente dopo ogni nuova misura.
- Il primo grafico mantiene 96 medie da 15 minuti.
- Il secondo grafico mantiene 30 medie giornaliere.
- Lo storico rimane disponibile dopo un riavvio grazie a LittleFS.
