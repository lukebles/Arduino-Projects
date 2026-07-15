# ESP32 Meteo in modalità Access Point

Il progetto usa:

- `WIFI_AP`, senza collegamento a un router;
- ESP-NOW sul canale 13;
- `WebServer` incluso nel core Arduino ESP32;
- LittleFS per `index.html`, `chart.min.js` e lo storico binario;
- Chart.js 2.9.3 eseguito localmente;
- ora e fuso orario inviati dal browser con `XMLHttpRequest`.

## Struttura della cartella

```text
esp32_meteo_ap/
├── esp32_meteo_ap.ino
└── data/
    ├── index.html
    └── chart.min.js
```

Copia la tua versione **Chart.js 2.9.3** in:

```text
data/chart.min.js
```

Il file non è incluso in questo archivio perché hai indicato di averlo già disponibile.

## Caricamento

1. Seleziona uno schema partizioni che riservi spazio a LittleFS.
2. Carica il contenuto della cartella `data` nella partizione LittleFS.
3. Carica lo sketch.
4. Collegati alla rete Wi-Fi `ESP32-Meteo`.
5. Password predefinita: `meteo1234`.
6. Apri `http://192.168.4.1/`.

Con PlatformIO puoi normalmente usare:

```bash
pio run -t uploadfs
pio run -t upload
```

Con Arduino IDE serve il relativo comando/plugin di caricamento LittleFS.

## Comportamento dell'orologio

Dopo ogni riavvio l'ESP32 non conosce l'ora finché non viene aperta la pagina.

All'apertura, il browser invia:

- Unix epoch UTC;
- differenza locale rispetto a UTC.

L'ESP32 continua poi a far avanzare l'orologio usando `esp_timer_get_time()`.
La pagina ripete la sincronizzazione ogni 5 minuti quando rimane aperta,
così corregge deriva e cambi dell'ora legale.

Le misure ricevute prima della prima sincronizzazione vengono mostrate come
ultima misura, ma non vengono inserite nelle medie storiche perché non possono
essere collocate correttamente nel tempo.

## Medie

- Grafico 1: 24 intervalli orari, compresa l'ora corrente parziale.
- Grafico 2: 30 intervalli giornalieri, compreso il giorno corrente parziale.
- Ogni punto è la media di tutti i pacchetti ricevuti nel relativo intervallo.
- Per un BMP280 l'umidità viene salvata come dato mancante e Chart.js mostra
  un'interruzione della linea.

## Persistenza

Lo storico viene salvato in `/history.bin`.

Il salvataggio avviene:

- subito alla chiusura di un'ora o di un giorno;
- al massimo ogni 10 minuti durante l'accumulo.

`index.html`, `chart.min.js` e `history.bin` condividono la stessa partizione
LittleFS.

## MAC ESP-NOW importante

In modalità `WIFI_AP`, lo sketch stampa:

```text
MAC AP da usare per ESP-NOW unicast: XX:XX:XX:XX:XX:XX
```

Se il trasmettitore invia in **unicast**, usa questo MAC come peer destinatario.
Non usare automaticamente il precedente MAC STA: normalmente MAC AP e MAC STA
sono differenti.

Con invio broadcast non è necessario modificare il destinatario.

## Compatibilità browser vecchi

La pagina evita funzionalità moderne come:

- `fetch`;
- Promise;
- funzioni freccia;
- `let` e `const`.

Usa `XMLHttpRequest` e sintassi JavaScript ES5, più adatta a browser del
2012-2013. La compatibilità finale dipende comunque dal browser specifico e
dalla build di Chart.js 2.9.3 utilizzata.
