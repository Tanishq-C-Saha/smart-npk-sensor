#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "esp_sleep.h"
#include <ModbusMaster.h>

#define debug_mode 1  // 0 = disable logs

#define RXD2 18  // ESP32 RX pin (connect to Arduino TX)
#define TXD2 19  // ESP32 TX pin (connect to Arduino RX)

/* ------------------- RS485 (UART1) ------------------- */
#define RXD1 16  // ESP32 RX1
#define TXD1 17  // ESP32 TX1
#define RE_DE 4  // RS485 direction control

/* ------------------- MODBUS ------------------- */
#define SOIL_SENSOR_ID 1
#define SOIL_REG_START 0x0000
#define SOIL_REG_COUNT 7

ModbusMaster node;

/* ---------------------------------------------------------------------
   LOG MESSAGE ID GUIDE (ESP32)
   ---------------------------------------------------------------------   
   Log IDs are grouped by severity.
   Use these IDs when sending logs over Serial / Serial2 / Network.

   FORMAT RECOMMENDED:
     $LOG,<ID>#

   LEVELS:
     INFO    = 0
     SUCCESS = 1
     WARNING = 2
     ERROR   = 3

   ===================================================================== */

/* =========================== INFO (0 – 10) ===========================

   0  : Booting system
   1  : System ready
   2  : Waiting for WiFi
   3  : WiFi connecting
   4  : Time sync started
   5  : Fetching config
   6  : Using cached config
   7  : Inside working hours
   8  : Outside working hours
   9  : Preparing sleep
   10 : Waking from deep sleep

   ===================================================================== */

/* ========================= SUCCESS (11 – 19) ==========================

   11 : WiFi connected
   12 : Time synchronized
   13 : Config fetched
   14 : Config applied
   15 : Sensor read OK
   16 : Data uploaded
   17 : Upload acknowledged
   18 : RTC time valid
   19 : Sleep scheduled

   ===================================================================== */

/* ========================= WARNING (20 – 28) ==========================

   20 : WiFi not connected
   21 : Weak WiFi signal
   22 : Config fetch skipped
   23 : Using default config
   24 : Sensor read delayed
   25 : Upload skipped: WiFi not connected
   26 : Retrying upload
   27 : Low voltage detected
   28 : Time not yet valid

   ===================================================================== */

/* =========================== ERROR (29 – 36) ==========================

   29 : WiFi connection failed
   30 : Time sync failed
   31 : Config fetch failed
   32 : JSON parse error
   33 : Upload failed
   34 : HTTP error
   35 : Invalid config received
   36 : RTC data corrupted

   ===================================================================== */

/* ------------------- WIFI ------------------- */
const char* ssid = "your_wifi_name";
const char* password = "your_wifi_pass";
#define WIFI_BLOCK_RETRY_DELAY 500  // 0.5 sec
#define WIFI_BLOCK_MAX_RETRIES 80   // ~40 sec total

/* ------------------- SERVER ------------------- */
const char* configURL = "https://script.google.com/macros/s/...../exec";   // your script link
const char* dataUploadURL = "https://script.google.com/macros/s/...../exec"; // your script link

/* ------------------- NTP ------------------- */
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;  // IST
const int daylightOffset_sec = 0;

/* ------------------- time interval ------------------- */
unsigned long lastSensorRead = 0;

/* ------------------- CONFIG STRUCT ------------------- */
typedef struct {
  int intervalSec;
  int startSec;
  int endSec;
  bool valid;
} Config_t;

/* ------------------- DEFAULT CONFIG ------------------- */
const Config_t DEFAULT_CONFIG = {
  .intervalSec = 300,
  .startSec = 9 * 3600,
  .endSec = 17 * 3600,
  .valid = true
};

/* ------------------- RTC MEMORY ------------------- */
RTC_DATA_ATTR Config_t rtcConfig;
RTC_DATA_ATTR time_t rtcBaseEpoch = 0;
RTC_DATA_ATTR unsigned long rtcBaseMillis = 0;
RTC_DATA_ATTR bool timeValid = false;
RTC_DATA_ATTR time_t lastConfigFetch = 0;

/* ------------------- SENSOR DATA STRUCT ------------------- */
struct SensorData {
  float temperature;
  float moisture;
  float conductivity;
  float pH;
  int nitrogen;
  int phosphorus;
  int potassium;
  bool valid;
};


/* ------------------- SETUP ------------------- */
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  sendLogIDSerial2(0);  //Booting System
  delay(2000);

  loadConfig();
  connectWiFi();
  syncTime();
  fetchConfig();
  sendLogIDSerial2(1);  // System Ready
  initSoilSensor();
}


void loop() {

  time_t now = getCurrentTime();
  int nowSec = getSecondsToday(now);
  ensureWiFiConnectedBlocking();

  if (nowSec >= rtcConfig.startSec && nowSec < rtcConfig.endSec) {
    if ((millis() - lastSensorRead) > rtcConfig.intervalSec * 1000UL) {
      sendLogIDSerial2(7);  // Inside working hours
      if (debug_mode) {
        Serial.printf("WORKING | %02d:%02d:%02d | %d sec\n",
                      nowSec / 3600,
                      (nowSec % 3600) / 60,
                      nowSec % 60,
                      nowSec);
      }
      uploadSensorData();
      lastSensorRead = millis();
    }
    return;
  }
  if (debug_mode) {
    Serial.printf("Outside Working Hours : Going to Deep Sleep (Time : %d)\n", nowSec);
  }
  sendLogIDSerial2(8);  // Outside working hours
  sendLogIDSerial2(9);  // Preparing sleep
  int sleepSec = calculateSleepUntilStart(nowSec);
  disconnectWiFi();
  sendLogIDSerial2(19);  // Sleep scheduled
  scheduleSleep(sleepSec);
}

/* ------------------- CONFIG ------------------- */
void loadConfig() {
  if (!rtcConfig.valid) {
    rtcConfig = DEFAULT_CONFIG;
    sendLogIDSerial2(23);  // Using default config
  } else {
    sendLogIDSerial2(6);  // Using cached config
  }
}

void fetchConfig() {
  time_t now = getCurrentTime();
  sendLogIDSerial2(5);  // Fetching config

  // Throttle config fetch (1 hr => 1 * 60 min * 60 sec)

  if (timeValid && (now - lastConfigFetch < 3600)) {
    if (debug_mode) Serial.println("Config up to date, skipping fetch");
    sendLogIDSerial2(22);  //Config fetch skipped
    return;
  }

  // WiFi must be connected
  if (!ensureWiFiConnectedBlocking()) {
    if (debug_mode) {
      Serial.println("WARNING: WiFi not connected, cannot fetch config");
    }
    sendLogIDSerial2(29);  // WiFi connection failed
    return;
  }

  HTTPClient http;
  http.begin(configURL);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpResponseCode = http.GET();

  if (httpResponseCode == HTTP_CODE_OK) {
    String payload = http.getString();

    if (debug_mode) {
      Serial.println("Received config:");
      Serial.println(payload);
    }

    if (parseAndSaveConfig(payload)) {
      lastConfigFetch = now;
      sendLogIDSerial2(13);  // Config fetched
      if (debug_mode) {
        Serial.println("Config parsed and saved");
      }
    }
  } else {
    sendLogIDSerial2(31);  //Config fetch failed
    if (debug_mode) {
      Serial.print("Config fetch failed, HTTP code: ");
      Serial.println(httpResponseCode);
    }
    // DO NOT touch existing rtcConfig
  }
  http.end();
}


/* ------------------- JSON PARSE ------------------- */
bool parseAndSaveConfig(const String& payload) {
  StaticJsonDocument<256> doc;

  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    if (debug_mode) {
      Serial.print("JSON parse error: ");
      Serial.println(err.c_str());
    }
    return false;
  }

  // Validate and apply with fallbacks
  rtcConfig.intervalSec = doc["interval_sec"] | rtcConfig.intervalSec;

  if (doc["start_time"] && doc["end_time"]) {
    rtcConfig.startSec = parseTime(doc["start_time"]);
    rtcConfig.endSec = parseTime(doc["end_time"]);
  } else {
    if (debug_mode) {
      Serial.println("WARNING: Missing start/end time in config");
    }
    return false;
  }

  rtcConfig.valid = true;
  return true;
}


/* ------------------- TIME ------------------- */
bool syncTime() {
  sendLogIDSerial2(4);  // Time sync started

  struct tm t;
  uint8_t retry = 0;
  const uint8_t MAX_RETRIES = 15;

  while (retry < MAX_RETRIES) {

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    if (getLocalTime(&t, 10000)) {
      time(&rtcBaseEpoch);
      rtcBaseMillis = millis();
      timeValid = true;

      sendLogIDSerial2(12);  // Time synchronized
      sendRTCEpochSerial2(rtcBaseEpoch);

      Serial2.flush();
      delay(100);

      if (debug_mode) {
        Serial.printf("RTC Base Epoch : %ld\n", rtcBaseEpoch);
      }

      return true;
    }

    retry++;
    sendLogIDSerial2(28);  // Time not yet valid
    delay(1000);
    yield();
  }

  sendLogIDSerial2(30);  // Time sync failed
  timeValid = false;
  return false;
}



time_t getCurrentTime() {
  return rtcBaseEpoch + (millis() - rtcBaseMillis) / 1000;
}

int getSecondsToday(time_t now) {
  struct tm t;
  localtime_r(&now, &t);
  return t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
}

/* ------------------- WIFI ------------------- */
void connectWiFi() {
  if (WiFi.isConnected()) return;

  WiFi.begin(ssid, password);
  if (debug_mode) {
    Serial.print("Connecting");
  }
  sendLogIDSerial2(3);  // WiFi Connecting
  while (WiFi.status() != WL_CONNECTED) {
    if (debug_mode) {
      Serial.print(".");
    }
    delay(500);
    sendLogIDSerial2(2);  // Waiting for WiFi
  }
  if (debug_mode) {
    Serial.println("\nWiFi Connected");
  }
  sendLogIDSerial2(11);  //WiFi connected
  delay(500);
}

void disconnectWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

/* ------------------- SENSOR ------------------- */
void runSensorTask() {
  float temp = random(200, 350) / 10.0;
  float hum = random(300, 800) / 10.0;

  Serial.printf("Temp: %.1f  Hum: %.1f\n", temp, hum);
}

void uploadSensorData() {


  //SensorData d = readFakeSensors();
  SensorData d = readSoilSensor();
  if (d.valid) {
    sendSensorFrameSerial2(d);

    StaticJsonDocument<256> doc;
    doc["N"] = d.nitrogen;
    doc["P"] = d.phosphorus;
    doc["K"] = d.potassium;
    doc["temperature"] = d.temperature;
    doc["moisture"] = d.moisture;
    doc["pH"] = d.pH;
    doc["conductivity"] = d.conductivity;

    String payload;
    serializeJson(doc, payload);

    if (!ensureWiFiConnectedBlocking()) {
      if (debug_mode) {
        Serial.println("Upload skipped: WiFi not connected");
      }
      sendLogIDSerial2(25);  // Upload skipped: WiFi not connected
      return;
    }

    HTTPClient http;
    http.begin(dataUploadURL);  // reuse your Apps Script URL
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");

    int code = http.POST(payload);
    if (code == HTTP_CODE_OK || 400) {
      sendLogIDSerial2(16);  // Data uploaded
    } else {
      sendLogIDSerial2(33);  // Upload failed
    }

    if (debug_mode) {
      if (code == HTTP_CODE_OK || 400) {
        Serial.println("Sensor data uploaded");
      } else {
        Serial.print("Upload failed, code: ");
        Serial.println(code);
      }
    }

    http.end();
  }
}

SensorData readFakeSensors() {
  SensorData d;

  d.nitrogen = random(10, 100);             // mg/kg
  d.phosphorus = random(5, 60);             // mg/kg
  d.potassium = random(20, 200);            // mg/kg
  d.temperature = random(150, 400) / 10.0;  // °C
  d.moisture = random(10, 100);
  d.pH = random(10, 100);
  d.conductivity = random(200, 2000);  // µS/cm
  d.valid = true;

  if (debug_mode) {
    Serial.printf("N : %d | ", d.nitrogen);
    Serial.printf("P : %d | ", d.phosphorus);
    Serial.printf("K : %d | ", d.potassium);
    Serial.printf("temperature : %.2f | ", d.temperature);
    Serial.printf("moisture : %d | ", d.moisture);
    Serial.printf("pH : %d | ", d.pH);
    Serial.printf("conductivity : %.2f | \n", d.conductivity);
  }
  sendLogIDSerial2(15);  // Sensor read OK
  return d;
}


/* ------------------- SLEEP ------------------- */
void scheduleSleep(int sec) {
  if (sec < 30) sec = 30;   // safety minimum
  digitalWrite(RE_DE, LOW);
  Serial1.end();

  esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);
  esp_deep_sleep_start();
}

/* ------------------- UTILS ------------------- */
int parseTime(const char* str) {
  int h = 0, m = 0;
  sscanf(str, "%d:%d", &h, &m);
  return h * 3600 + m * 60;
}

int calculateSleepUntilStart(int nowSec) {
  if (nowSec < rtcConfig.startSec)
    return rtcConfig.startSec - nowSec;

  return (24 * 3600 - nowSec) + rtcConfig.startSec;
}



/* ------------------- SENSOR PACKET GENERATOR ------------------- */

void sendSensorFrameSerial2(const SensorData& d) {
  char frame[128];

  snprintf(frame, sizeof(frame),
           "$SENS,%d,%d,%d,%.2f,%.2f,%.2f,%.2f#",
           d.nitrogen,
           d.phosphorus,
           d.potassium,
           d.temperature,
           d.moisture,
           d.pH,
           d.conductivity);

  Serial2.println(frame);

  if (debug_mode) {
    Serial.print("Serial2 TX -> ");
    Serial.println(frame);
  }
}

/* ------------------- TIME RTC BASE EPOCH PACKET GENERATOR ------------------- */
void sendRTCEpochSerial2(time_t epoch) {
  char frame[64];

  snprintf(frame, sizeof(frame),
           "$TIME,%ld#",
           (long)epoch);

  Serial2.println(frame);

  if (debug_mode) {
    Serial.print("Serial2 TX -> ");
    Serial.println(frame);
  }
}

/* ------------------- LOG PACKET GENERATOR ------------------- */
void sendLogIDSerial2(int logID) {
  char frame[64];

  snprintf(frame, sizeof(frame),
           "$LOG,%d#",
           logID);

  Serial2.println(frame);

  if (debug_mode) {
    Serial.print("Serial2 TX -> ");
    Serial.println(frame);
  }
}


/* ------------------- RS485 DIRECTION ------------------- */
void preTransmission() {
  digitalWrite(RE_DE, HIGH);
}

void postTransmission() {
  digitalWrite(RE_DE, LOW);
}

/* -------------------SENSOR INIT ------------------- */
void initSoilSensor() {
  Serial1.begin(9600, SERIAL_8N1, RXD1, TXD1);

  pinMode(RE_DE, OUTPUT);
  digitalWrite(RE_DE, LOW);  // RX mode

  node.begin(SOIL_SENSOR_ID, Serial1);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
}



/* ------------------- READ SENSOR ------------------- */
SensorData readSoilSensor() {
  SensorData d;
  d.valid = false;
  uint8_t result = 0;

  for (int i = 0; i < 3; i++) {
    result = node.readHoldingRegisters(SOIL_REG_START, SOIL_REG_COUNT);
    if (result == node.ku8MBSuccess) break;
    delay(100);
  }

  if (result != node.ku8MBSuccess) {
    Serial.print("Modbus error: ");
    Serial.println(result);
    sendLogIDSerial2(24);  // Sensor read delayed
    return d;
  }

  d.temperature = node.getResponseBuffer(0) / 10.0;
  d.moisture = node.getResponseBuffer(1) / 10.0;
  d.conductivity = node.getResponseBuffer(2);
  d.pH = node.getResponseBuffer(3) / 100.0;
  d.nitrogen = node.getResponseBuffer(4);
  d.phosphorus = node.getResponseBuffer(5);
  d.potassium = node.getResponseBuffer(6);

  d.valid = true;
  sendLogIDSerial2(15);  //  Sensor read OK
  if (debug_mode) {

    printSoilSensor(d);
  }
  return d;
}


/* ------------------- DEBUG PRINT ------------------- */
void printSoilSensor(const SensorData& d) {
  if (!d.valid) {
    Serial.println("Soil sensor data invalid");
    return;
  }

  Serial.println("----- Soil Sensor Readings -----");
  Serial.print("Temperature: ");
  Serial.print(d.temperature);
  Serial.println(" °C");
  Serial.print("Moisture:    ");
  Serial.print(d.moisture);
  Serial.println(" %");
  Serial.print("Conductivity:");
  Serial.print(d.conductivity);
  Serial.println(" µS/cm");
  Serial.print("pH Value:    ");
  Serial.println(d.pH);
  Serial.print("Nitrogen:    ");
  Serial.print(d.nitrogen);
  Serial.println(" mg/kg");
  Serial.print("Phosphorus:  ");
  Serial.print(d.phosphorus);
  Serial.println(" mg/kg");
  Serial.print("Potassium:   ");
  Serial.print(d.potassium);
  Serial.println(" mg/kg");
  Serial.println("--------------------------------\n");
}


/* ------------------- WiFi RECONNECT ------------------- */

bool ensureWiFiConnectedBlocking() {
  if (WiFi.isConnected()) return true;

  sendLogIDSerial2(20);  // WiFi not connected
  delay(500);
  sendLogIDSerial2(3);  // WiFi connecting
  delay(500);

  if (debug_mode) {
    Serial.println("WiFi lost! Blocking until reconnection...");
  }

  WiFi.disconnect(true);
  WiFi.begin(ssid, password);

  uint8_t retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_BLOCK_RETRY_DELAY);
    sendLogIDSerial2(2);  // Waiting For Wifi

    retry++;

    if (debug_mode) Serial.print(".");

    if (retry >= WIFI_BLOCK_MAX_RETRIES) {
      sendLogIDSerial2(29);  // WiFi connection failed

      if (debug_mode) {
        Serial.println("\nWiFi reconnect failed (timeout)");
      }

      return false;
    }
  }

  sendLogIDSerial2(11);  // WiFi connected
  delay(500);
  if (debug_mode) {
    Serial.println("\nWiFi reconnected");
  }

  return true;
}
