#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <time.h>

// ======================================================
// CONFIGURAZIONE
// ======================================================

#define ESPNOW_CHANNEL 13

#define NODE_TX1 1
#define TYPE_ENVIRONMENT 10

#define SENSOR_UNKNOWN 0
#define SENSOR_BME280  1
#define SENSOR_BMP280  2

const char *AP_SSID     = "ESP32-Meteo";
const char *AP_PASSWORD = "meteo1234";   // minimo 8 caratteri

constexpr uint8_t MAX_AP_CLIENTS = 4;

constexpr size_t HOURLY_POINTS = 24;
constexpr size_t DAILY_POINTS  = 30;

constexpr uint32_t STORAGE_MAGIC   = 0x4D455445; // "METE"
constexpr uint16_t STORAGE_VERSION = 1;

// Salvataggio periodico dello stato corrente.
// Riducilo se vuoi perdere meno campioni in caso di spegnimento improvviso.
constexpr int64_t SAVE_INTERVAL_US = 10LL * 60LL * 1000000LL;

// ======================================================
// FORMATO PACCHETTO AMBIENTALE
// ======================================================
//
// temp_x100  = temperatura °C x 100
// hum_x100   = umidità % x 100, oppure -1 se non disponibile
// press_x100 = pressione hPa x 100
//

struct __attribute__((packed)) PacketEnvironment {
  uint8_t nodeId;
  uint8_t payloadType;
  uint32_t seq;
  int16_t temp_x100;
  int16_t hum_x100;
  uint32_t press_x100;
  uint8_t sensorType;
};

// ======================================================
// STRUTTURE INTERNE
// ======================================================

struct ReceivedMeasurement {
  uint8_t sourceMac[6];
  uint32_t seq;
  float temperature;
  float humidity;
  float pressure;
  uint8_t sensorType;
  uint8_t humidityValid;
};

enum ValueFlags : uint8_t {
  VALUE_TEMP  = 1 << 0,
  VALUE_HUM   = 1 << 1,
  VALUE_PRESS = 1 << 2
};

struct HistoryPoint {
  uint32_t bucketStartLocal;
  float temperature;
  float humidity;
  float pressure;
  uint8_t validFlags;
  uint8_t reserved[3];
};

struct Accumulator {
  uint32_t bucketStartLocal;

  double temperatureSum;
  double humiditySum;
  double pressureSum;

  uint32_t temperatureCount;
  uint32_t humidityCount;
  uint32_t pressureCount;

  uint8_t active;
  uint8_t reserved[3];
};

struct PersistedState {
  uint32_t magic;
  uint16_t version;
  uint16_t structSize;

  HistoryPoint hourly[HOURLY_POINTS];
  uint8_t hourlyCount;
  uint8_t hourlyHead;
  uint8_t reservedHourly[2];

  HistoryPoint daily[DAILY_POINTS];
  uint8_t dailyCount;
  uint8_t dailyHead;
  uint8_t reservedDaily[2];

  Accumulator currentHour;
  Accumulator currentDay;
};

struct LatestMeasurement {
  uint32_t localEpoch;
  uint32_t seq;
  float temperature;
  float humidity;
  float pressure;
  uint8_t sensorType;
  uint8_t humidityValid;
  uint8_t valid;
  uint8_t reserved;
};

// ======================================================
// GLOBALI
// ======================================================

WebServer server(80);
QueueHandle_t receivedQueue = nullptr;

PersistedState state;
LatestMeasurement latestMeasurement = {};

bool historyDirty = false;
bool forceHistorySave = false;
int64_t lastHistorySaveUs = 0;

// L'orologio è impostato dal browser.
// clockBaseLocalEpoch contiene già la correzione del fuso orario.
bool clockValid = false;
uint64_t clockBaseLocalEpoch = 0;
int64_t clockBaseMicros = 0;
int32_t browserUtcOffsetMinutes = 0;

uint32_t lastRolloverCheckMs = 0;

// ======================================================
// UTILITY
// ======================================================

void stampaMacBytes(const uint8_t *mac) {
  char macStr[18];

  snprintf(
    macStr,
    sizeof(macStr),
    "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2],
    mac[3], mac[4], mac[5]
  );

  Serial.print(macStr);
}

const char *nomeSensore(uint8_t sensorType) {
  switch (sensorType) {
    case SENSOR_BME280:
      return "BME280";
    case SENSOR_BMP280:
      return "BMP280";
    default:
      return "Sconosciuto";
  }
}

uint32_t currentLocalEpoch() {
  if (!clockValid) {
    return 0;
  }

  int64_t elapsedMicros = esp_timer_get_time() - clockBaseMicros;
  uint64_t elapsedSeconds = static_cast<uint64_t>(elapsedMicros / 1000000LL);

  return static_cast<uint32_t>(clockBaseLocalEpoch + elapsedSeconds);
}

void setClockFromBrowser(uint32_t utcEpoch, int32_t utcOffsetMinutes) {
  browserUtcOffsetMinutes = utcOffsetMinutes;

  int64_t localEpoch =
    static_cast<int64_t>(utcEpoch) +
    static_cast<int64_t>(utcOffsetMinutes) * 60LL;

  if (localEpoch < 0) {
    localEpoch = 0;
  }

  clockBaseLocalEpoch = static_cast<uint64_t>(localEpoch);
  clockBaseMicros = esp_timer_get_time();
  clockValid = true;
}

String formatLocalEpoch(uint32_t localEpoch, const char *format) {
  time_t raw = static_cast<time_t>(localEpoch);
  struct tm tmValue;

  gmtime_r(&raw, &tmValue);

  char buffer[32];
  strftime(buffer, sizeof(buffer), format, &tmValue);

  return String(buffer);
}

String jsonFloat(float value, bool valid, uint8_t decimals = 2) {
  if (!valid || isnan(value) || isinf(value)) {
    return "null";
  }

  return String(value, static_cast<unsigned int>(decimals));
}

// ======================================================
// STORICO SU LITTLEFS
// ======================================================

void resetState() {
  memset(&state, 0, sizeof(state));

  state.magic = STORAGE_MAGIC;
  state.version = STORAGE_VERSION;
  state.structSize = sizeof(PersistedState);
}

bool loadState() {
  if (!LittleFS.exists("/history.bin")) {
    Serial.println("Storico non ancora presente");
    resetState();
    return false;
  }

  File file = LittleFS.open("/history.bin", "r");

  if (!file) {
    Serial.println("Impossibile aprire /history.bin");
    resetState();
    return false;
  }

  if (file.size() != sizeof(PersistedState)) {
    Serial.println("Dimensione storico non compatibile: inizializzazione");
    file.close();
    resetState();
    return false;
  }

  size_t bytesRead = file.read(
    reinterpret_cast<uint8_t *>(&state),
    sizeof(PersistedState)
  );

  file.close();

  if (
    bytesRead != sizeof(PersistedState) ||
    state.magic != STORAGE_MAGIC ||
    state.version != STORAGE_VERSION ||
    state.structSize != sizeof(PersistedState)
  ) {
    Serial.println("Storico non valido: inizializzazione");
    resetState();
    return false;
  }

  if (
    state.hourlyCount > HOURLY_POINTS ||
    state.hourlyHead >= HOURLY_POINTS ||
    state.dailyCount > DAILY_POINTS ||
    state.dailyHead >= DAILY_POINTS
  ) {
    Serial.println("Indici dello storico non validi: inizializzazione");
    resetState();
    return false;
  }

  Serial.print("Storico caricato: ");
  Serial.print(state.hourlyCount);
  Serial.print(" valori orari, ");
  Serial.print(state.dailyCount);
  Serial.println(" valori giornalieri");

  return true;
}

bool saveState() {
  state.magic = STORAGE_MAGIC;
  state.version = STORAGE_VERSION;
  state.structSize = sizeof(PersistedState);

  File file = LittleFS.open("/history.tmp", "w");

  if (!file) {
    Serial.println("Errore apertura /history.tmp");
    return false;
  }

  size_t bytesWritten = file.write(
    reinterpret_cast<const uint8_t *>(&state),
    sizeof(PersistedState)
  );

  file.flush();
  file.close();

  if (bytesWritten != sizeof(PersistedState)) {
    Serial.println("Scrittura incompleta dello storico");
    LittleFS.remove("/history.tmp");
    return false;
  }

  LittleFS.remove("/history.bin");

  if (!LittleFS.rename("/history.tmp", "/history.bin")) {
    Serial.println("Errore rinomina dello storico");
    return false;
  }

  historyDirty = false;
  forceHistorySave = false;
  lastHistorySaveUs = esp_timer_get_time();

  Serial.println("Storico salvato su LittleFS");
  return true;
}

void markHistoryDirty(bool immediate = false) {
  historyDirty = true;

  if (immediate) {
    forceHistorySave = true;
  }
}

// ======================================================
// BUFFER CIRCOLARI
// ======================================================

void pushHistoryPoint(
  HistoryPoint *buffer,
  size_t capacity,
  uint8_t &count,
  uint8_t &head,
  const HistoryPoint &point
) {
  if (count > 0) {
    size_t newestIndex = (head + capacity - 1) % capacity;
    HistoryPoint &newest = buffer[newestIndex];

    if (newest.bucketStartLocal == point.bucketStartLocal) {
      newest = point;
      return;
    }

    // Evita di inserire punti fuori ordine in seguito
    // a una correzione indietro dell'orologio.
    if (point.bucketStartLocal < newest.bucketStartLocal) {
      return;
    }
  }

  buffer[head] = point;
  head = static_cast<uint8_t>((head + 1) % capacity);

  if (count < capacity) {
    count++;
  }
}

bool findHistoryPoint(
  const HistoryPoint *buffer,
  size_t capacity,
  uint8_t count,
  uint8_t head,
  uint32_t bucketStart,
  HistoryPoint &result
) {
  if (count == 0) {
    return false;
  }

  size_t oldestIndex = (head + capacity - count) % capacity;

  for (size_t i = 0; i < count; i++) {
    size_t index = (oldestIndex + i) % capacity;

    if (buffer[index].bucketStartLocal == bucketStart) {
      result = buffer[index];
      return true;
    }
  }

  return false;
}

// ======================================================
// AGGREGAZIONE
// ======================================================

void startAccumulator(Accumulator &accumulator, uint32_t bucketStart) {
  memset(&accumulator, 0, sizeof(accumulator));
  accumulator.bucketStartLocal = bucketStart;
  accumulator.active = 1;
}

void addToAccumulator(
  Accumulator &accumulator,
  float temperature,
  bool humidityValid,
  float humidity,
  float pressure
) {
  accumulator.temperatureSum += temperature;
  accumulator.temperatureCount++;

  if (humidityValid) {
    accumulator.humiditySum += humidity;
    accumulator.humidityCount++;
  }

  accumulator.pressureSum += pressure;
  accumulator.pressureCount++;
}

HistoryPoint pointFromAccumulator(const Accumulator &accumulator) {
  HistoryPoint point = {};
  point.bucketStartLocal = accumulator.bucketStartLocal;

  if (accumulator.temperatureCount > 0) {
    point.temperature = static_cast<float>(
      accumulator.temperatureSum / accumulator.temperatureCount
    );
    point.validFlags |= VALUE_TEMP;
  }

  if (accumulator.humidityCount > 0) {
    point.humidity = static_cast<float>(
      accumulator.humiditySum / accumulator.humidityCount
    );
    point.validFlags |= VALUE_HUM;
  }

  if (accumulator.pressureCount > 0) {
    point.pressure = static_cast<float>(
      accumulator.pressureSum / accumulator.pressureCount
    );
    point.validFlags |= VALUE_PRESS;
  }

  return point;
}

void finalizeHour() {
  if (!state.currentHour.active) {
    return;
  }

  HistoryPoint point = pointFromAccumulator(state.currentHour);

  if (point.validFlags != 0) {
    pushHistoryPoint(
      state.hourly,
      HOURLY_POINTS,
      state.hourlyCount,
      state.hourlyHead,
      point
    );
  }

  memset(&state.currentHour, 0, sizeof(state.currentHour));
  markHistoryDirty(true);
}

void finalizeDay() {
  if (!state.currentDay.active) {
    return;
  }

  HistoryPoint point = pointFromAccumulator(state.currentDay);

  if (point.validFlags != 0) {
    pushHistoryPoint(
      state.daily,
      DAILY_POINTS,
      state.dailyCount,
      state.dailyHead,
      point
    );
  }

  memset(&state.currentDay, 0, sizeof(state.currentDay));
  markHistoryDirty(true);
}

void updateAccumulatorForBucket(
  Accumulator &accumulator,
  uint32_t bucketStart,
  bool hourly,
  float temperature,
  bool humidityValid,
  float humidity,
  float pressure
) {
  if (!accumulator.active) {
    startAccumulator(accumulator, bucketStart);
  } else if (bucketStart > accumulator.bucketStartLocal) {
    if (hourly) {
      finalizeHour();
      startAccumulator(state.currentHour, bucketStart);
    } else {
      finalizeDay();
      startAccumulator(state.currentDay, bucketStart);
    }
  } else if (bucketStart < accumulator.bucketStartLocal) {
    // L'orologio del browser è stato corretto all'indietro.
    // Si riparte dal nuovo intervallo senza creare punti fuori ordine.
    startAccumulator(accumulator, bucketStart);
  }

  addToAccumulator(
    accumulator,
    temperature,
    humidityValid,
    humidity,
    pressure
  );
}

void rolloverAccumulatorsIfNeeded() {
  if (!clockValid) {
    return;
  }

  uint32_t nowLocal = currentLocalEpoch();
  uint32_t hourStart = (nowLocal / 3600UL) * 3600UL;
  uint32_t dayStart = (nowLocal / 86400UL) * 86400UL;

  if (
    state.currentHour.active &&
    hourStart > state.currentHour.bucketStartLocal
  ) {
    finalizeHour();
  }

  if (
    state.currentDay.active &&
    dayStart > state.currentDay.bucketStartLocal
  ) {
    finalizeDay();
  }
}

bool getPointForBucket(
  bool hourly,
  uint32_t bucketStart,
  HistoryPoint &result
) {
  bool found;

  if (hourly) {
    found = findHistoryPoint(
      state.hourly,
      HOURLY_POINTS,
      state.hourlyCount,
      state.hourlyHead,
      bucketStart,
      result
    );

    if (
      state.currentHour.active &&
      state.currentHour.bucketStartLocal == bucketStart
    ) {
      result = pointFromAccumulator(state.currentHour);
      return result.validFlags != 0;
    }
  } else {
    found = findHistoryPoint(
      state.daily,
      DAILY_POINTS,
      state.dailyCount,
      state.dailyHead,
      bucketStart,
      result
    );

    if (
      state.currentDay.active &&
      state.currentDay.bucketStartLocal == bucketStart
    ) {
      result = pointFromAccumulator(state.currentDay);
      return result.validFlags != 0;
    }
  }

  return found;
}

void processMeasurement(const ReceivedMeasurement &measurement) {
  Serial.println();
  Serial.println("======================================");

  Serial.print("Ricevuto da MAC: ");
  stampaMacBytes(measurement.sourceMac);
  Serial.println();

  Serial.print("Nodo: TX");
  Serial.println(NODE_TX1);

  Serial.print("Sensore: ");
  Serial.println(nomeSensore(measurement.sensorType));

  Serial.print("Sequenza: ");
  Serial.println(measurement.seq);

  Serial.print("Temperatura: ");
  Serial.print(measurement.temperature, 2);
  Serial.println(" °C");

  if (measurement.humidityValid) {
    Serial.print("Umidità: ");
    Serial.print(measurement.humidity, 2);
    Serial.println(" %");
  } else {
    Serial.println("Umidità: non disponibile");
  }

  Serial.print("Pressione: ");
  Serial.print(measurement.pressure, 2);
  Serial.println(" hPa");

  latestMeasurement.seq = measurement.seq;
  latestMeasurement.temperature = measurement.temperature;
  latestMeasurement.humidity = measurement.humidity;
  latestMeasurement.pressure = measurement.pressure;
  latestMeasurement.sensorType = measurement.sensorType;
  latestMeasurement.humidityValid = measurement.humidityValid;
  latestMeasurement.valid = 1;
  latestMeasurement.localEpoch = clockValid ? currentLocalEpoch() : 0;

  if (!clockValid) {
    Serial.println(
      "Misura visualizzabile, ma non archiviata: "
      "ora non ancora ricevuta dal browser"
    );
    return;
  }

  uint32_t nowLocal = currentLocalEpoch();
  uint32_t hourStart = (nowLocal / 3600UL) * 3600UL;
  uint32_t dayStart = (nowLocal / 86400UL) * 86400UL;

  updateAccumulatorForBucket(
    state.currentHour,
    hourStart,
    true,
    measurement.temperature,
    measurement.humidityValid,
    measurement.humidity,
    measurement.pressure
  );

  updateAccumulatorForBucket(
    state.currentDay,
    dayStart,
    false,
    measurement.temperature,
    measurement.humidityValid,
    measurement.humidity,
    measurement.pressure
  );

  markHistoryDirty(false);
}

// ======================================================
// CALLBACK ESP-NOW
// ======================================================

void onDataRecv(
  const esp_now_recv_info_t *recvInfo,
  const uint8_t *incomingData,
  int len
) {
  if (len != static_cast<int>(sizeof(PacketEnvironment))) {
    return;
  }

  PacketEnvironment packet;
  memcpy(&packet, incomingData, sizeof(packet));

  if (
    packet.nodeId != NODE_TX1 ||
    packet.payloadType != TYPE_ENVIRONMENT
  ) {
    return;
  }

  ReceivedMeasurement measurement = {};

  memcpy(measurement.sourceMac, recvInfo->src_addr, 6);

  measurement.seq = packet.seq;
  measurement.temperature = packet.temp_x100 / 100.0f;
  measurement.pressure = packet.press_x100 / 100.0f;
  measurement.sensorType = packet.sensorType;

  if (packet.hum_x100 >= 0) {
    measurement.humidity = packet.hum_x100 / 100.0f;
    measurement.humidityValid = 1;
  }

  if (xQueueSend(receivedQueue, &measurement, 0) != pdTRUE) {
    // Se la coda è piena, elimina la misura più vecchia.
    ReceivedMeasurement discarded;
    xQueueReceive(receivedQueue, &discarded, 0);
    xQueueSend(receivedQueue, &measurement, 0);
  }
}

// ======================================================
// JSON PER I GRAFICI
// ======================================================

void appendLabelsArray(
  String &json,
  bool hourly,
  uint32_t firstBucket,
  size_t pointCount
) {
  json += "\"labels\":[";

  uint32_t step = hourly ? 3600UL : 86400UL;
  const char *format = hourly ? "%d/%m %H:00" : "%d/%m";

  for (size_t i = 0; i < pointCount; i++) {
    if (i > 0) {
      json += ',';
    }

    uint32_t bucket = firstBucket + static_cast<uint32_t>(i) * step;

    json += '"';
    json += formatLocalEpoch(bucket, format);
    json += '"';
  }

  json += ']';
}

void appendMetricArray(
  String &json,
  const char *name,
  bool hourly,
  uint32_t firstBucket,
  size_t pointCount,
  uint8_t metricFlag
) {
  json += ",\"";
  json += name;
  json += "\":[";

  uint32_t step = hourly ? 3600UL : 86400UL;

  for (size_t i = 0; i < pointCount; i++) {
    if (i > 0) {
      json += ',';
    }

    uint32_t bucket = firstBucket + static_cast<uint32_t>(i) * step;
    HistoryPoint point = {};

    if (!getPointForBucket(hourly, bucket, point)) {
      json += "null";
      continue;
    }

    bool valid = (point.validFlags & metricFlag) != 0;

    if (metricFlag == VALUE_TEMP) {
      json += jsonFloat(point.temperature, valid);
    } else if (metricFlag == VALUE_HUM) {
      json += jsonFloat(point.humidity, valid);
    } else {
      json += jsonFloat(point.pressure, valid);
    }
  }

  json += ']';
}

void appendWindowJson(
  String &json,
  const char *name,
  bool hourly,
  uint32_t currentBucket,
  size_t pointCount
) {
  uint32_t step = hourly ? 3600UL : 86400UL;
  uint32_t firstBucket =
    currentBucket - static_cast<uint32_t>(pointCount - 1) * step;

  json += ",\"";
  json += name;
  json += "\":{";

  appendLabelsArray(json, hourly, firstBucket, pointCount);
  appendMetricArray(
    json,
    "temperature",
    hourly,
    firstBucket,
    pointCount,
    VALUE_TEMP
  );
  appendMetricArray(
    json,
    "humidity",
    hourly,
    firstBucket,
    pointCount,
    VALUE_HUM
  );
  appendMetricArray(
    json,
    "pressure",
    hourly,
    firstBucket,
    pointCount,
    VALUE_PRESS
  );

  json += '}';
}

String buildHistoryJson() {
  String json;
  json.reserve(9000);

  json += '{';
  json += "\"timeValid\":";
  json += clockValid ? "true" : "false";

  json += ",\"utcOffsetMinutes\":";
  json += String(browserUtcOffsetMinutes);

  if (clockValid) {
    uint32_t nowLocal = currentLocalEpoch();

    json += ",\"deviceTime\":\"";
    json += formatLocalEpoch(nowLocal, "%d/%m/%Y %H:%M:%S");
    json += '"';

    uint32_t currentHour = (nowLocal / 3600UL) * 3600UL;
    uint32_t currentDay = (nowLocal / 86400UL) * 86400UL;

    appendWindowJson(
      json,
      "hourly",
      true,
      currentHour,
      HOURLY_POINTS
    );

    appendWindowJson(
      json,
      "daily",
      false,
      currentDay,
      DAILY_POINTS
    );
  } else {
    json += ",\"deviceTime\":null";
    json += ",\"hourly\":null";
    json += ",\"daily\":null";
  }

  json += ",\"latest\":{";
  json += "\"valid\":";
  json += latestMeasurement.valid ? "true" : "false";

  if (latestMeasurement.valid) {
    json += ",\"sequence\":";
    json += String(latestMeasurement.seq);

    json += ",\"sensor\":\"";
    json += nomeSensore(latestMeasurement.sensorType);
    json += '"';

    json += ",\"temperature\":";
    json += jsonFloat(latestMeasurement.temperature, true);

    json += ",\"humidity\":";
    json += jsonFloat(
      latestMeasurement.humidity,
      latestMeasurement.humidityValid
    );

    json += ",\"pressure\":";
    json += jsonFloat(latestMeasurement.pressure, true);

    json += ",\"time\":";
    if (latestMeasurement.localEpoch > 0) {
      json += '"';
      json += formatLocalEpoch(
        latestMeasurement.localEpoch,
        "%d/%m/%Y %H:%M:%S"
      );
      json += '"';
    } else {
      json += "null";
    }
  }

  json += '}';

  json += '}';
  return json;
}

// ======================================================
// SERVER WEB
// ======================================================

void sendFileFromLittleFS(
  const char *path,
  const char *contentType,
  bool cacheLong
) {
  File file = LittleFS.open(path, "r");

  if (!file) {
    server.send(
      404,
      "text/plain; charset=utf-8",
      String("File non trovato: ") + path
    );
    return;
  }

  if (cacheLong) {
    server.sendHeader("Cache-Control", "public, max-age=31536000");
  } else {
    server.sendHeader("Cache-Control", "no-cache");
  }

  server.streamFile(file, contentType);
  file.close();
}

void handleRoot() {
  sendFileFromLittleFS(
    "/index.html",
    "text/html; charset=utf-8",
    false
  );
}

void handleChartJs() {
  sendFileFromLittleFS(
    "/chart.min.js",
    "application/javascript; charset=utf-8",
    true
  );
}

void handleTimeSync() {
  if (!server.hasArg("epoch") || !server.hasArg("offset")) {
    server.send(
      400,
      "application/json",
      "{\"ok\":false,\"error\":\"Parametri mancanti\"}"
    );
    return;
  }

  uint32_t utcEpoch = strtoul(server.arg("epoch").c_str(), nullptr, 10);
  int32_t offsetMinutes = server.arg("offset").toInt();

  // Controlli volutamente ampi.
  if (
    utcEpoch < 1609459200UL ||       // 01/01/2021
    offsetMinutes < -840 ||
    offsetMinutes > 840
  ) {
    server.send(
      400,
      "application/json",
      "{\"ok\":false,\"error\":\"Data o fuso non validi\"}"
    );
    return;
  }

  setClockFromBrowser(utcEpoch, offsetMinutes);
  rolloverAccumulatorsIfNeeded();

  String response = "{\"ok\":true,\"deviceTime\":\"";
  response += formatLocalEpoch(
    currentLocalEpoch(),
    "%d/%m/%Y %H:%M:%S"
  );
  response += "\"}";

  server.send(200, "application/json", response);

  Serial.print("Ora sincronizzata dal browser: ");
  Serial.println(
    formatLocalEpoch(
      currentLocalEpoch(),
      "%d/%m/%Y %H:%M:%S"
    )
  );
}

void handleHistoryApi() {
  rolloverAccumulatorsIfNeeded();

  server.sendHeader("Cache-Control", "no-cache");
  server.send(
    200,
    "application/json; charset=utf-8",
    buildHistoryJson()
  );
}

void handleNotFound() {
  server.send(
    404,
    "text/plain; charset=utf-8",
    "Risorsa non trovata"
  );
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/index.html", HTTP_GET, handleRoot);
  server.on("/chart.min.js", HTTP_GET, handleChartJs);
  server.on("/api/time", HTTP_POST, handleTimeSync);
  server.on("/api/history", HTTP_GET, handleHistoryApi);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Server web avviato");
}

// ======================================================
// ACCESS POINT ED ESP-NOW
// ======================================================

bool setupAccessPoint() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  delay(200);

  // In Italia sono ammessi i canali 1-13.
  // Impostazione manuale utile per garantire il canale 13.
  wifi_country_t country = {};
  country.cc[0] = 'I';
  country.cc[1] = 'T';
  country.cc[2] = '\0';
  country.schan = 1;
  country.nchan = 13;
  country.policy = WIFI_COUNTRY_POLICY_MANUAL;
  esp_wifi_set_country(&country);

  bool ok = WiFi.softAP(
    AP_SSID,
    AP_PASSWORD,
    ESPNOW_CHANNEL,
    false,
    MAX_AP_CLIENTS
  );

  uint8_t macSta[6];
  uint8_t macAp[6];

  esp_wifi_get_mac(WIFI_IF_STA, macSta);
  esp_wifi_get_mac(WIFI_IF_AP, macAp);

  Serial.print("MAC STA usato da ESP-NOW: ");
  stampaMacBytes(macSta);
  Serial.println();

  Serial.print("MAC AP usato dalla pagina web: ");
  stampaMacBytes(macAp);
  Serial.println();

  if (!ok) {
    return false;
  }

  delay(300);

  Serial.print("SSID access point: ");
  Serial.println(AP_SSID);

  Serial.print("IP pagina web: http://");
  Serial.println(WiFi.softAPIP());

  Serial.print("MAC AP da usare per ESP-NOW unicast: ");
  Serial.println(WiFi.softAPmacAddress());

  uint8_t primaryChannel = 0;
  wifi_second_chan_t secondaryChannel = WIFI_SECOND_CHAN_NONE;

  if (
    esp_wifi_get_channel(
      &primaryChannel,
      &secondaryChannel
    ) == ESP_OK
  ) {
    Serial.print("Canale Wi-Fi/ESP-NOW effettivo: ");
    Serial.println(primaryChannel);
  }

  return true;
}

bool setupEspNow() {
  if (esp_now_init() != ESP_OK) {
    return false;
  }

  esp_now_register_recv_cb(onDataRecv);
  return true;
}

// ======================================================
// SETUP
// ======================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println(
    "CENTRALE ESP-NOW + ACCESS POINT + STORICO METEO"
  );

  if (!LittleFS.begin(true)) {
    Serial.println("Errore inizializzazione LittleFS");

    while (true) {
      delay(1000);
    }
  }

  Serial.print("LittleFS totale: ");
  Serial.print(LittleFS.totalBytes());
  Serial.print(" byte, usati: ");
  Serial.print(LittleFS.usedBytes());
  Serial.println(" byte");

  loadState();
  lastHistorySaveUs = esp_timer_get_time();

  receivedQueue = xQueueCreate(
    8,
    sizeof(ReceivedMeasurement)
  );

  if (receivedQueue == nullptr) {
    Serial.println("Errore creazione coda ricezione");

    while (true) {
      delay(1000);
    }
  }

  if (!setupAccessPoint()) {
    Serial.println("Errore avvio access point");

    while (true) {
      delay(1000);
    }
  }

  if (!setupEspNow()) {
    Serial.println("Errore inizializzazione ESP-NOW");

    while (true) {
      delay(1000);
    }
  }

  setupWebServer();

  Serial.println("Centrale pronta");
  Serial.println(
    "Dopo ogni riavvio apri la pagina web almeno una volta "
    "per impostare data e ora."
  );
}

// ======================================================
// LOOP
// ======================================================

void loop() {
  server.handleClient();

  ReceivedMeasurement measurement;

  while (
    xQueueReceive(
      receivedQueue,
      &measurement,
      0
    ) == pdTRUE
  ) {
    processMeasurement(measurement);
  }

  uint32_t nowMs = millis();

  if (nowMs - lastRolloverCheckMs >= 1000UL) {
    lastRolloverCheckMs = nowMs;
    rolloverAccumulatorsIfNeeded();
  }

  if (historyDirty) {
    int64_t nowUs = esp_timer_get_time();

    if (
      forceHistorySave ||
      nowUs - lastHistorySaveUs >= SAVE_INTERVAL_US
    ) {
      saveState();
    }
  }

  delay(2);
}
