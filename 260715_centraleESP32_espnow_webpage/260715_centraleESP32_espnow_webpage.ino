#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>

#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <esp_err.h>

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

const char *AP_SSID = "ESP32-Meteo";
const char *AP_PASSWORD = "meteo1234";

constexpr uint8_t MAX_AP_CLIENTS = 4;
constexpr uint16_t HTTP_PORT = 80;
constexpr uint16_t WEBSOCKET_PORT = 81;

// 24 ore con un punto ogni 15 minuti.
constexpr size_t QUARTER_HOUR_POINTS = 96;

// Ultimi 30 giorni.
constexpr size_t DAILY_POINTS = 30;

constexpr uint32_t QUARTER_HOUR_SECONDS = 15UL * 60UL;
constexpr uint32_t DAY_SECONDS = 24UL * 60UL * 60UL;

constexpr uint32_t STORAGE_MAGIC = 0x4D455445;
constexpr uint16_t STORAGE_VERSION = 2;

// Salva l'intervallo corrente al massimo ogni 15 minuti.
constexpr int64_t SAVE_INTERVAL_US =
  15LL * 60LL * 1000000LL;

// ======================================================
// PACCHETTO ESP-NOW
// ======================================================

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
  VALUE_TEMP = 1 << 0,
  VALUE_HUM = 1 << 1,
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

  HistoryPoint quarterHour[QUARTER_HOUR_POINTS];
  uint16_t quarterHourCount;

  HistoryPoint daily[DAILY_POINTS];
  uint16_t dailyCount;

  Accumulator currentQuarterHour;
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

WebServer server(HTTP_PORT);
WebSocketsServer webSocket(WEBSOCKET_PORT);

QueueHandle_t receivedQueue = nullptr;

PersistedState state;
LatestMeasurement latestMeasurement = {};

bool historyDirty = false;
bool forceHistorySave = false;

int64_t lastHistorySaveUs = 0;

// Ora locale impostata dal browser.
bool clockValid = false;

uint64_t clockBaseLocalEpoch = 0;
int64_t clockBaseMicros = 0;

int32_t browserUtcOffsetMinutes = 0;

uint32_t lastRolloverCheckMs = 0;


volatile uint32_t espNowCallbackCount = 0;
volatile int espNowLastPacketLength = 0;

// ======================================================
// UTILITY
// ======================================================

void stampaMacBytes(const uint8_t *mac) {
  char macStr[18];

  snprintf(
    macStr,
    sizeof(macStr),
    "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0],
    mac[1],
    mac[2],
    mac[3],
    mac[4],
    mac[5]
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

  int64_t elapsedMicros =
    esp_timer_get_time() -
    clockBaseMicros;

  uint64_t elapsedSeconds =
    static_cast<uint64_t>(
      elapsedMicros / 1000000LL
    );

  return static_cast<uint32_t>(
    clockBaseLocalEpoch +
    elapsedSeconds
  );
}

void setClockFromBrowser(
  uint32_t utcEpoch,
  int32_t utcOffsetMinutes
) {
  browserUtcOffsetMinutes =
    utcOffsetMinutes;

  int64_t localEpoch =
    static_cast<int64_t>(utcEpoch) +
    static_cast<int64_t>(
      utcOffsetMinutes
    ) *
    60LL;

  if (localEpoch < 0) {
    localEpoch = 0;
  }

  clockBaseLocalEpoch =
    static_cast<uint64_t>(
      localEpoch
    );

  clockBaseMicros =
    esp_timer_get_time();

  clockValid = true;
}

String formatLocalEpoch(
  uint32_t localEpoch,
  const char *format
) {
  time_t rawTime =
    static_cast<time_t>(
      localEpoch
    );

  struct tm timeInfo;

  gmtime_r(
    &rawTime,
    &timeInfo
  );

  char buffer[32];

  strftime(
    buffer,
    sizeof(buffer),
    format,
    &timeInfo
  );

  return String(buffer);
}

String jsonFloat(
  float value,
  bool valid,
  unsigned int decimals = 2
) {
  if (
    !valid ||
    isnan(value) ||
    isinf(value)
  ) {
    return "null";
  }

  return String(
    value,
    decimals
  );
}

// ======================================================
// LITTLEFS E PERSISTENZA
// ======================================================

void resetState() {
  memset(
    &state,
    0,
    sizeof(state)
  );

  state.magic =
    STORAGE_MAGIC;

  state.version =
    STORAGE_VERSION;

  state.structSize =
    sizeof(PersistedState);
}

bool validateState() {
  return
    state.magic ==
      STORAGE_MAGIC &&
    state.version ==
      STORAGE_VERSION &&
    state.structSize ==
      sizeof(PersistedState) &&
    state.quarterHourCount <=
      QUARTER_HOUR_POINTS &&
    state.dailyCount <=
      DAILY_POINTS;
}

bool loadState() {
  if (
    !LittleFS.exists(
      "/history.bin"
    )
  ) {
    Serial.println(
      "Storico non ancora presente"
    );

    resetState();
    return false;
  }

  File file =
    LittleFS.open(
      "/history.bin",
      "r"
    );

  if (!file) {
    Serial.println(
      "Impossibile aprire /history.bin"
    );

    resetState();
    return false;
  }

  if (
    file.size() !=
    sizeof(PersistedState)
  ) {
    Serial.println(
      "Storico di formato precedente: "
      "verrà reinizializzato"
    );

    file.close();

    LittleFS.remove(
      "/history.bin"
    );

    resetState();
    return false;
  }

  size_t bytesRead =
    file.read(
      reinterpret_cast<uint8_t *>(
        &state
      ),
      sizeof(PersistedState)
    );

  file.close();

  if (
    bytesRead !=
      sizeof(PersistedState) ||
    !validateState()
  ) {
    Serial.println(
      "Storico non valido: "
      "verrà reinizializzato"
    );

    LittleFS.remove(
      "/history.bin"
    );

    resetState();
    return false;
  }

  Serial.print(
    "Storico caricato: "
  );

  Serial.print(
    state.quarterHourCount
  );

  Serial.print(
    " intervalli da 15 minuti, "
  );

  Serial.print(
    state.dailyCount
  );

  Serial.println(
    " intervalli giornalieri"
  );

  return true;
}

bool saveState() {
  state.magic =
    STORAGE_MAGIC;

  state.version =
    STORAGE_VERSION;

  state.structSize =
    sizeof(PersistedState);

  File file =
    LittleFS.open(
      "/history.tmp",
      "w"
    );

  if (!file) {
    Serial.println(
      "Errore apertura /history.tmp"
    );

    return false;
  }

  size_t bytesWritten =
    file.write(
      reinterpret_cast<const uint8_t *>(
        &state
      ),
      sizeof(PersistedState)
    );

  file.flush();
  file.close();

  if (
    bytesWritten !=
    sizeof(PersistedState)
  ) {
    Serial.println(
      "Scrittura incompleta dello storico"
    );

    LittleFS.remove(
      "/history.tmp"
    );

    return false;
  }

  LittleFS.remove(
    "/history.bin"
  );

  if (
    !LittleFS.rename(
      "/history.tmp",
      "/history.bin"
    )
  ) {
    Serial.println(
      "Errore rinomina dello storico"
    );

    return false;
  }

  historyDirty = false;
  forceHistorySave = false;

  lastHistorySaveUs =
    esp_timer_get_time();

  Serial.println(
    "Storico salvato su LittleFS"
  );

  return true;
}

void markHistoryDirty(
  bool immediate = false
) {
  historyDirty = true;

  if (immediate) {
    forceHistorySave = true;
  }
}

// ======================================================
// ARRAY STORICI
// ======================================================

void appendHistoryPoint(
  HistoryPoint *buffer,
  size_t capacity,
  uint16_t &count,
  const HistoryPoint &point
) {
  if (count > 0) {
    HistoryPoint &last =
      buffer[count - 1];

    if (
      last.bucketStartLocal ==
      point.bucketStartLocal
    ) {
      last = point;
      return;
    }

    if (
      point.bucketStartLocal <
      last.bucketStartLocal
    ) {
      return;
    }
  }

  if (count < capacity) {
    buffer[count] = point;
    count++;
    return;
  }

  memmove(
    &buffer[0],
    &buffer[1],
    (capacity - 1) *
      sizeof(HistoryPoint)
  );

  buffer[capacity - 1] =
    point;
}

bool findHistoryPoint(
  const HistoryPoint *buffer,
  uint16_t count,
  uint32_t bucketStart,
  HistoryPoint &result
) {
  for (
    uint16_t i = 0;
    i < count;
    i++
  ) {
    if (
      buffer[i].bucketStartLocal ==
      bucketStart
    ) {
      result = buffer[i];
      return true;
    }
  }

  return false;
}

// ======================================================
// MEDIE
// ======================================================

void startAccumulator(
  Accumulator &accumulator,
  uint32_t bucketStart
) {
  memset(
    &accumulator,
    0,
    sizeof(accumulator)
  );

  accumulator.bucketStartLocal =
    bucketStart;

  accumulator.active = 1;
}

void addToAccumulator(
  Accumulator &accumulator,
  float temperature,
  bool humidityValid,
  float humidity,
  float pressure
) {
  accumulator.temperatureSum +=
    temperature;

  accumulator.temperatureCount++;

  if (humidityValid) {
    accumulator.humiditySum +=
      humidity;

    accumulator.humidityCount++;
  }

  accumulator.pressureSum +=
    pressure;

  accumulator.pressureCount++;
}

HistoryPoint pointFromAccumulator(
  const Accumulator &accumulator
) {
  HistoryPoint point = {};

  point.bucketStartLocal =
    accumulator.bucketStartLocal;

  if (
    accumulator.temperatureCount > 0
  ) {
    point.temperature =
      static_cast<float>(
        accumulator.temperatureSum /
        accumulator.temperatureCount
      );

    point.validFlags |=
      VALUE_TEMP;
  }

  if (
    accumulator.humidityCount > 0
  ) {
    point.humidity =
      static_cast<float>(
        accumulator.humiditySum /
        accumulator.humidityCount
      );

    point.validFlags |=
      VALUE_HUM;
  }

  if (
    accumulator.pressureCount > 0
  ) {
    point.pressure =
      static_cast<float>(
        accumulator.pressureSum /
        accumulator.pressureCount
      );

    point.validFlags |=
      VALUE_PRESS;
  }

  return point;
}

void finalizeQuarterHour() {
  if (
    !state.currentQuarterHour.active
  ) {
    return;
  }

  HistoryPoint point =
    pointFromAccumulator(
      state.currentQuarterHour
    );

  if (point.validFlags != 0) {
    appendHistoryPoint(
      state.quarterHour,
      QUARTER_HOUR_POINTS,
      state.quarterHourCount,
      point
    );
  }

  memset(
    &state.currentQuarterHour,
    0,
    sizeof(state.currentQuarterHour)
  );

  markHistoryDirty(true);
}

void finalizeDay() {
  if (
    !state.currentDay.active
  ) {
    return;
  }

  HistoryPoint point =
    pointFromAccumulator(
      state.currentDay
    );

  if (point.validFlags != 0) {
    appendHistoryPoint(
      state.daily,
      DAILY_POINTS,
      state.dailyCount,
      point
    );
  }

  memset(
    &state.currentDay,
    0,
    sizeof(state.currentDay)
  );

  markHistoryDirty(true);
}

void updateQuarterHourAccumulator(
  uint32_t bucketStart,
  float temperature,
  bool humidityValid,
  float humidity,
  float pressure
) {
  if (
    !state.currentQuarterHour.active
  ) {
    startAccumulator(
      state.currentQuarterHour,
      bucketStart
    );
  } else if (
    bucketStart >
    state.currentQuarterHour.bucketStartLocal
  ) {
    finalizeQuarterHour();

    startAccumulator(
      state.currentQuarterHour,
      bucketStart
    );
  } else if (
    bucketStart <
    state.currentQuarterHour.bucketStartLocal
  ) {
    startAccumulator(
      state.currentQuarterHour,
      bucketStart
    );
  }

  addToAccumulator(
    state.currentQuarterHour,
    temperature,
    humidityValid,
    humidity,
    pressure
  );
}

void updateDayAccumulator(
  uint32_t bucketStart,
  float temperature,
  bool humidityValid,
  float humidity,
  float pressure
) {
  if (
    !state.currentDay.active
  ) {
    startAccumulator(
      state.currentDay,
      bucketStart
    );
  } else if (
    bucketStart >
    state.currentDay.bucketStartLocal
  ) {
    finalizeDay();

    startAccumulator(
      state.currentDay,
      bucketStart
    );
  } else if (
    bucketStart <
    state.currentDay.bucketStartLocal
  ) {
    startAccumulator(
      state.currentDay,
      bucketStart
    );
  }

  addToAccumulator(
    state.currentDay,
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

  uint32_t nowLocal =
    currentLocalEpoch();

  uint32_t quarterStart =
    (
      nowLocal /
      QUARTER_HOUR_SECONDS
    ) *
    QUARTER_HOUR_SECONDS;

  uint32_t dayStart =
    (
      nowLocal /
      DAY_SECONDS
    ) *
    DAY_SECONDS;

  if (
    state.currentQuarterHour.active &&
    quarterStart >
      state.currentQuarterHour
        .bucketStartLocal
  ) {
    finalizeQuarterHour();
  }

  if (
    state.currentDay.active &&
    dayStart >
      state.currentDay
        .bucketStartLocal
  ) {
    finalizeDay();
  }
}

bool getPointForBucket(
  bool quarterHour,
  uint32_t bucketStart,
  HistoryPoint &result
) {
  if (quarterHour) {
    if (
      state.currentQuarterHour.active &&
      state.currentQuarterHour
        .bucketStartLocal ==
      bucketStart
    ) {
      result =
        pointFromAccumulator(
          state.currentQuarterHour
        );

      return
        result.validFlags != 0;
    }

    return findHistoryPoint(
      state.quarterHour,
      state.quarterHourCount,
      bucketStart,
      result
    );
  }

  if (
    state.currentDay.active &&
    state.currentDay
      .bucketStartLocal ==
    bucketStart
  ) {
    result =
      pointFromAccumulator(
        state.currentDay
      );

    return
      result.validFlags != 0;
  }

  return findHistoryPoint(
    state.daily,
    state.dailyCount,
    bucketStart,
    result
  );
}

// ======================================================
// ELABORAZIONE MISURA
// ======================================================

void processMeasurement(
  const ReceivedMeasurement &measurement
) {
  Serial.println();

  Serial.println(
    "======================================"
  );

  Serial.print(
    "Ricevuto da MAC: "
  );

  stampaMacBytes(
    measurement.sourceMac
  );

  Serial.println();

  Serial.print("Nodo: TX");
  Serial.println(NODE_TX1);

  Serial.print("Sensore: ");

  Serial.println(
    nomeSensore(
      measurement.sensorType
    )
  );

  Serial.print("Sequenza: ");
  Serial.println(
    measurement.seq
  );

  Serial.print("Temperatura: ");

  Serial.print(
    measurement.temperature,
    2
  );

  Serial.println(" °C");

  if (
    measurement.humidityValid
  ) {
    Serial.print("Umidità: ");

    Serial.print(
      measurement.humidity,
      2
    );

    Serial.println(" %");
  } else {
    Serial.println(
      "Umidità: non disponibile"
    );
  }

  Serial.print("Pressione: ");

  Serial.print(
    measurement.pressure,
    2
  );

  Serial.println(" hPa");

  latestMeasurement.seq =
    measurement.seq;

  latestMeasurement.temperature =
    measurement.temperature;

  latestMeasurement.humidity =
    measurement.humidity;

  latestMeasurement.pressure =
    measurement.pressure;

  latestMeasurement.sensorType =
    measurement.sensorType;

  latestMeasurement.humidityValid =
    measurement.humidityValid;

  latestMeasurement.valid = 1;

  latestMeasurement.localEpoch =
    clockValid
      ? currentLocalEpoch()
      : 0;

  if (clockValid) {
    uint32_t nowLocal =
      currentLocalEpoch();

    uint32_t quarterStart =
      (
        nowLocal /
        QUARTER_HOUR_SECONDS
      ) *
      QUARTER_HOUR_SECONDS;

    uint32_t dayStart =
      (
        nowLocal /
        DAY_SECONDS
      ) *
      DAY_SECONDS;

    updateQuarterHourAccumulator(
      quarterStart,
      measurement.temperature,
      measurement.humidityValid,
      measurement.humidity,
      measurement.pressure
    );

    updateDayAccumulator(
      dayStart,
      measurement.temperature,
      measurement.humidityValid,
      measurement.humidity,
      measurement.pressure
    );

    markHistoryDirty(false);
  } else {
    Serial.println(
      "Misura non archiviata: "
      "ora non ancora ricevuta "
      "dal browser"
    );
  }

  // Avvisa immediatamente i browser collegati.
  webSocket.broadcastTXT(
    "measurement"
  );
}

// ======================================================
// CALLBACK ESP-NOW
// ======================================================

void onDataRecv(
  const esp_now_recv_info_t *recvInfo,
  const uint8_t *incomingData,
  int len
) {
  espNowCallbackCount++;
  espNowLastPacketLength = len;

  if (
    len !=
    static_cast<int>(
      sizeof(PacketEnvironment)
    )
  ) {
    return;
  }

  PacketEnvironment packet;

  memcpy(
    &packet,
    incomingData,
    sizeof(packet)
  );

  if (
    packet.nodeId != NODE_TX1 ||
    packet.payloadType !=
      TYPE_ENVIRONMENT
  ) {
    return;
  }

  ReceivedMeasurement measurement = {};

  memcpy(
    measurement.sourceMac,
    recvInfo->src_addr,
    6
  );

  measurement.seq =
    packet.seq;

  measurement.temperature =
    packet.temp_x100 /
    100.0f;

  measurement.pressure =
    packet.press_x100 /
    100.0f;

  measurement.sensorType =
    packet.sensorType;

  if (
    packet.hum_x100 >= 0
  ) {
    measurement.humidity =
      packet.hum_x100 /
      100.0f;

    measurement.humidityValid = 1;
  }

  if (
    xQueueSend(
      receivedQueue,
      &measurement,
      0
    ) != pdTRUE
  ) {
    ReceivedMeasurement discarded;

    xQueueReceive(
      receivedQueue,
      &discarded,
      0
    );

    xQueueSend(
      receivedQueue,
      &measurement,
      0
    );
  }
}

// ======================================================
// CREAZIONE JSON
// ======================================================

void appendLabelsArray(
  String &json,
  bool quarterHour,
  uint32_t firstBucket,
  size_t pointCount
) {
  json += "\"labels\":[";

  uint32_t step =
    quarterHour
      ? QUARTER_HOUR_SECONDS
      : DAY_SECONDS;

  const char *format =
    quarterHour
      ? "%d/%m %H:%M"
      : "%d/%m";

  for (
    size_t i = 0;
    i < pointCount;
    i++
  ) {
    if (i > 0) {
      json += ',';
    }

    uint32_t bucket =
      firstBucket +
      static_cast<uint32_t>(i) *
      step;

    json += '"';

    json += formatLocalEpoch(
      bucket,
      format
    );

    json += '"';
  }

  json += ']';
}

void appendMetricArray(
  String &json,
  const char *name,
  bool quarterHour,
  uint32_t firstBucket,
  size_t pointCount,
  uint8_t metricFlag
) {
  json += ",\"";
  json += name;
  json += "\":[";

  uint32_t step =
    quarterHour
      ? QUARTER_HOUR_SECONDS
      : DAY_SECONDS;

  for (
    size_t i = 0;
    i < pointCount;
    i++
  ) {
    if (i > 0) {
      json += ',';
    }

    uint32_t bucket =
      firstBucket +
      static_cast<uint32_t>(i) *
      step;

    HistoryPoint point = {};

    if (
      !getPointForBucket(
        quarterHour,
        bucket,
        point
      )
    ) {
      json += "null";
      continue;
    }

    bool valid =
      (
        point.validFlags &
        metricFlag
      ) != 0;

    if (
      metricFlag ==
      VALUE_TEMP
    ) {
      json += jsonFloat(
        point.temperature,
        valid
      );
    } else if (
      metricFlag ==
      VALUE_HUM
    ) {
      json += jsonFloat(
        point.humidity,
        valid
      );
    } else {
      json += jsonFloat(
        point.pressure,
        valid
      );
    }
  }

  json += ']';
}

void appendWindowJson(
  String &json,
  const char *name,
  bool quarterHour,
  uint32_t currentBucket,
  size_t pointCount
) {
  uint32_t step =
    quarterHour
      ? QUARTER_HOUR_SECONDS
      : DAY_SECONDS;

  uint32_t firstBucket =
    currentBucket -
    static_cast<uint32_t>(
      pointCount - 1
    ) *
    step;

  json += ",\"";
  json += name;
  json += "\":{";

  appendLabelsArray(
    json,
    quarterHour,
    firstBucket,
    pointCount
  );

  appendMetricArray(
    json,
    "temperature",
    quarterHour,
    firstBucket,
    pointCount,
    VALUE_TEMP
  );

  appendMetricArray(
    json,
    "humidity",
    quarterHour,
    firstBucket,
    pointCount,
    VALUE_HUM
  );

  appendMetricArray(
    json,
    "pressure",
    quarterHour,
    firstBucket,
    pointCount,
    VALUE_PRESS
  );

  json += '}';
}

String buildHistoryJson() {
  String json;

  json.reserve(18000);

  json += '{';

  json += "\"timeValid\":";

  json +=
    clockValid
      ? "true"
      : "false";

  json +=
    ",\"utcOffsetMinutes\":";

  json += String(
    browserUtcOffsetMinutes
  );

  if (clockValid) {
    uint32_t nowLocal =
      currentLocalEpoch();

    json +=
      ",\"deviceTime\":\"";

    json += formatLocalEpoch(
      nowLocal,
      "%d/%m/%Y %H:%M:%S"
    );

    json += '"';

    uint32_t currentQuarter =
      (
        nowLocal /
        QUARTER_HOUR_SECONDS
      ) *
      QUARTER_HOUR_SECONDS;

    uint32_t currentDay =
      (
        nowLocal /
        DAY_SECONDS
      ) *
      DAY_SECONDS;

    appendWindowJson(
      json,
      "quarterHour",
      true,
      currentQuarter,
      QUARTER_HOUR_POINTS
    );

    appendWindowJson(
      json,
      "daily",
      false,
      currentDay,
      DAILY_POINTS
    );
  } else {
    json +=
      ",\"deviceTime\":null";

    json +=
      ",\"quarterHour\":null";

    json +=
      ",\"daily\":null";
  }

  json += ",\"latest\":{";

  json += "\"valid\":";

  json +=
    latestMeasurement.valid
      ? "true"
      : "false";

  if (
    latestMeasurement.valid
  ) {
    json += ",\"sequence\":";

    json += String(
      latestMeasurement.seq
    );

    json += ",\"sensor\":\"";

    json += nomeSensore(
      latestMeasurement.sensorType
    );

    json += '"';

    json += ",\"temperature\":";

    json += jsonFloat(
      latestMeasurement.temperature,
      true
    );

    json += ",\"humidity\":";

    json += jsonFloat(
      latestMeasurement.humidity,
      latestMeasurement.humidityValid
    );

    json += ",\"pressure\":";

    json += jsonFloat(
      latestMeasurement.pressure,
      true
    );

    json += ",\"time\":";

    if (
      latestMeasurement.localEpoch >
      0
    ) {
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

  json += ",\"espNow\":{";

  json += "\"callbackCount\":";

  json += String(
    espNowCallbackCount
  );

  json +=
    ",\"lastPacketLength\":";

  json += String(
    espNowLastPacketLength
  );

  json += '}';

  json += '}';

  return json;
}

// ======================================================
// SERVER HTTP
// ======================================================

void sendFileFromLittleFS(
  const char *path,
  const char *contentType,
  bool cacheLong
) {
  File file =
    LittleFS.open(
      path,
      "r"
    );

  if (!file) {
    server.send(
      404,
      "text/plain; charset=utf-8",
      String(
        "File non trovato: "
      ) +
      path
    );

    return;
  }

  if (cacheLong) {
    server.sendHeader(
      "Cache-Control",
      "public, max-age=31536000"
    );
  } else {
    server.sendHeader(
      "Cache-Control",
      "no-cache"
    );
  }

  server.streamFile(
    file,
    contentType
  );

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
  if (
    !server.hasArg("epoch") ||
    !server.hasArg("offset")
  ) {
    server.send(
      400,
      "application/json",
      "{\"ok\":false,"
      "\"error\":\"Parametri mancanti\"}"
    );

    return;
  }

  uint32_t utcEpoch =
    strtoul(
      server.arg("epoch").c_str(),
      nullptr,
      10
    );

  int32_t offsetMinutes =
    server.arg("offset").toInt();

  if (
    utcEpoch < 1609459200UL ||
    offsetMinutes < -840 ||
    offsetMinutes > 840
  ) {
    server.send(
      400,
      "application/json",
      "{\"ok\":false,"
      "\"error\":\"Data o fuso non validi\"}"
    );

    return;
  }

  setClockFromBrowser(
    utcEpoch,
    offsetMinutes
  );

  rolloverAccumulatorsIfNeeded();

  String response =
    "{\"ok\":true,"
    "\"deviceTime\":\"";

  response += formatLocalEpoch(
    currentLocalEpoch(),
    "%d/%m/%Y %H:%M:%S"
  );

  response += "\"}";

  server.send(
    200,
    "application/json",
    response
  );

  Serial.print(
    "Ora sincronizzata dal browser: "
  );

  Serial.println(
    formatLocalEpoch(
      currentLocalEpoch(),
      "%d/%m/%Y %H:%M:%S"
    )
  );

  webSocket.broadcastTXT(
    "refresh"
  );
}

void handleHistoryApi() {
  rolloverAccumulatorsIfNeeded();

  server.sendHeader(
    "Cache-Control",
    "no-cache"
  );

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
  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/index.html",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/chart.min.js",
    HTTP_GET,
    handleChartJs
  );

  server.on(
    "/api/time",
    HTTP_POST,
    handleTimeSync
  );

  server.on(
    "/api/history",
    HTTP_GET,
    handleHistoryApi
  );

  server.onNotFound(
    handleNotFound
  );

  server.begin();

  Serial.print(
    "Server HTTP avviato sulla porta "
  );

  Serial.println(
    HTTP_PORT
  );
}

// ======================================================
// SERVER WEBSOCKET
// ======================================================

void webSocketEvent(
  uint8_t clientNumber,
  WStype_t type,
  uint8_t *payload,
  size_t length
) {
  switch (type) {
    case WStype_CONNECTED: {
      IPAddress clientIp =
        webSocket.remoteIP(
          clientNumber
        );

      Serial.print(
        "WebSocket connesso, client "
      );

      Serial.print(
        clientNumber
      );

      Serial.print(" da ");

      Serial.println(
        clientIp
      );

      webSocket.sendTXT(
        clientNumber,
        "refresh"
      );

      break;
    }

    case WStype_DISCONNECTED:
      Serial.print(
        "WebSocket disconnesso, client "
      );

      Serial.println(
        clientNumber
      );

      break;

    case WStype_TEXT:
      (void)payload;
      (void)length;
      break;

    default:
      break;
  }
}

void setupWebSocketServer() {
  webSocket.begin();

  webSocket.onEvent(
    webSocketEvent
  );

  webSocket.enableHeartbeat(
    15000,
    3000,
    2
  );

  Serial.print(
    "Server WebSocket avviato sulla porta "
  );

  Serial.println(
    WEBSOCKET_PORT
  );
}

// ======================================================
// WIFI AP+STA ED ESP-NOW
// ======================================================

bool setupWiFiAndEspNow() {
  WiFi.persistent(false);

  WiFi.setAutoReconnect(
    false
  );

  /*
   * Cancella eventuali vecchie credenziali
   * Station e spegne temporaneamente il Wi-Fi.
   *
   * Non verrà chiamato WiFi.begin(),
   * quindi non ci sarà alcuna connessione
   * a un router.
   */
  WiFi.disconnect(
    true,
    true
  );

  delay(200);

  /*
   * STA mantiene il vecchio MAC ESP-NOW.
   * AP crea la rete della pagina web.
   */
  bool modeOk =
    WiFi.mode(
      WIFI_AP_STA
    );

  if (!modeOk) {
    Serial.println(
      "Errore impostazione modalità "
      "WIFI_AP_STA"
    );

    return false;
  }

  delay(300);

  /*
   * Abilita esplicitamente i canali
   * europei da 1 a 13.
   */
  wifi_country_t country = {};

  country.cc[0] = 'I';
  country.cc[1] = 'T';
  country.cc[2] = '\0';

  country.schan = 1;
  country.nchan = 13;

  country.policy =
    WIFI_COUNTRY_POLICY_MANUAL;

  esp_err_t countryResult =
    esp_wifi_set_country(
      &country
    );

  Serial.print(
    "Impostazione paese Wi-Fi IT: "
  );

  Serial.println(
    esp_err_to_name(
      countryResult
    )
  );

  if (
    countryResult != ESP_OK
  ) {
    return false;
  }

  /*
   * WiFi.softAP() imposta direttamente
   * il canale 13.
   *
   * Non viene chiamato esp_wifi_set_channel()
   * prima dell'avvio dell'AP.
   */
  bool apOk =
    WiFi.softAP(
      AP_SSID,
      AP_PASSWORD,
      ESPNOW_CHANNEL,
      false,
      MAX_AP_CLIENTS
    );

  if (!apOk) {
    Serial.println(
      "Errore avvio access point"
    );

    return false;
  }

  delay(500);

  // Evita il risparmio energetico Wi-Fi.
  esp_err_t powerSaveResult =
    esp_wifi_set_ps(
      WIFI_PS_NONE
    );

  Serial.print(
    "Disattivazione risparmio Wi-Fi: "
  );

  Serial.println(
    esp_err_to_name(
      powerSaveResult
    )
  );

  uint8_t actualChannel = 0;

  wifi_second_chan_t secondChannel =
    WIFI_SECOND_CHAN_NONE;

  esp_err_t channelResult =
    esp_wifi_get_channel(
      &actualChannel,
      &secondChannel
    );

  if (
    channelResult != ESP_OK
  ) {
    Serial.print(
      "Errore lettura canale: "
    );

    Serial.println(
      esp_err_to_name(
        channelResult
      )
    );

    return false;
  }

  Serial.print(
    "Canale Wi-Fi/ESP-NOW effettivo: "
  );

  Serial.println(
    actualChannel
  );

  if (
    actualChannel !=
    ESPNOW_CHANNEL
  ) {
    Serial.print(
      "ERRORE: era richiesto il canale "
    );

    Serial.println(
      ESPNOW_CHANNEL
    );

    return false;
  }

  uint8_t macSta[6];
  uint8_t macAp[6];

  esp_wifi_get_mac(
    WIFI_IF_STA,
    macSta
  );

  esp_wifi_get_mac(
    WIFI_IF_AP,
    macAp
  );

  Serial.print(
    "MAC STA usato da ESP-NOW: "
  );

  stampaMacBytes(
    macSta
  );

  Serial.println();

  Serial.print(
    "MAC AP della pagina web: "
  );

  stampaMacBytes(
    macAp
  );

  Serial.println();

  Serial.print(
    "SSID access point: "
  );

  Serial.println(
    AP_SSID
  );

  Serial.print(
    "IP pagina web: http://"
  );

  Serial.println(
    WiFi.softAPIP()
  );

  /*
   * ESP-NOW viene inizializzato soltanto
   * dopo che l'AP ha impostato il canale.
   */
  esp_err_t espNowResult =
    esp_now_init();

  if (
    espNowResult != ESP_OK
  ) {
    Serial.print(
      "Errore esp_now_init(): "
    );

    Serial.println(
      esp_err_to_name(
        espNowResult
      )
    );

    return false;
  }

  espNowResult =
    esp_now_register_recv_cb(
      onDataRecv
    );

  if (
    espNowResult != ESP_OK
  ) {
    Serial.print(
      "Errore registrazione callback "
      "ESP-NOW: "
    );

    Serial.println(
      esp_err_to_name(
        espNowResult
      )
    );

    return false;
  }

  Serial.println(
    "Callback ESP-NOW registrata"
  );

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
    "CENTRALE ESP-NOW + AP + "
    "WEBSOCKET + METEO"
  );

  if (
    !LittleFS.begin(true)
  ) {
    Serial.println(
      "Errore inizializzazione LittleFS"
    );

    while (true) {
      delay(1000);
    }
  }

  Serial.print(
    "LittleFS totale: "
  );

  Serial.print(
    LittleFS.totalBytes()
  );

  Serial.print(
    " byte, usati: "
  );

  Serial.print(
    LittleFS.usedBytes()
  );

  Serial.println(
    " byte"
  );

  loadState();

  lastHistorySaveUs =
    esp_timer_get_time();

  receivedQueue =
    xQueueCreate(
      8,
      sizeof(ReceivedMeasurement)
    );

  if (
    receivedQueue == nullptr
  ) {
    Serial.println(
      "Errore creazione coda ricezione"
    );

    while (true) {
      delay(1000);
    }
  }

  if (
    !setupWiFiAndEspNow()
  ) {
    Serial.println(
      "Errore inizializzazione "
      "Wi-Fi/ESP-NOW"
    );

    while (true) {
      delay(1000);
    }
  }

  setupWebServer();
  setupWebSocketServer();

  Serial.println(
    "Centrale pronta"
  );

  Serial.println(
    "Apri http://192.168.4.1/"
  );
}

// ======================================================
// LOOP
// ======================================================

void loop() {
  server.handleClient();
  webSocket.loop();

  ReceivedMeasurement measurement;

  while (
    xQueueReceive(
      receivedQueue,
      &measurement,
      0
    ) == pdTRUE
  ) {
    processMeasurement(
      measurement
    );
  }

  uint32_t nowMs =
    millis();

  if (
    nowMs -
      lastRolloverCheckMs >=
    1000UL
  ) {
    lastRolloverCheckMs =
      nowMs;

    rolloverAccumulatorsIfNeeded();
  }

  if (historyDirty) {
    int64_t nowUs =
      esp_timer_get_time();

    if (
      forceHistorySave ||
      nowUs -
        lastHistorySaveUs >=
      SAVE_INTERVAL_US
    ) {
      saveState();
    }
  }

  
  delay(2);
}