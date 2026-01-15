/*********************************************************************
 * Project : NPK Sensor Dashboard (Display Unit)
 * Board   : Arduino UNO 
 * Display : 3.5" TFT (MCUFRIEND_kbv)
 * Purpose : Visualize sensor data (NPK, Temp, Moisture, pH ,Logs etc.)
 * Author  : Tanishq Saha
 *********************************************************************/

#include <MCUFRIEND_kbv.h>
#include <Adafruit_GFX.h>
#include <time.h>


/* -----------  helper macro for CLEARING TEXT ----------------*/
#define CLEAR_TEXT(x, y, w, h) tft.fillRect(x, y, w, h, COLOR_BLACK)


/* ------------------- SENSOR DATA STRUCT ------------------- */
struct SensorData {
  float temperature;
  float moisture;
  float conductivity;
  float pH;
  int nitrogen;
  int phosphorus;
  int potassium;
};

SensorData SENSOR_DATA;


// ---------------- BAR PIXELS ----------------
int nBar = 0;
int pBar = 0;
int kBar = 0;

int tempBar = 0;
int phBar = 0;

// ---------------- SERIAL LOG SYSTEM ----------------
enum LogType {
  LOG_INFO,
  LOG_SUCCESS,
  LOG_WARNING
};

// ---------------- SERIAL BUFFER VARIABLES ----------------
char serialBuffer[64];
bool packetReady = false;

// ---------------- TIME SYSTEM ----------------
#define IST_OFFSET 19800UL  // 5h 30m in seconds
unsigned long baseMillis = 0;
unsigned long baseEpoch = 0;
bool timeValid = false;
uint8_t hh, mm, mo, dd;
uint16_t yy;

// ---------------- WiFi SYSTEM ----------------
enum WifiState {
  WIFI_IDLE,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_FAILED
};

WifiState wifiState = WIFI_IDLE;
WifiState lastWifiState = WIFI_IDLE;
uint16_t lastWifiColor;
unsigned long wifiBlinkTimer = 0;
bool wifiBlinkOn = false;
const unsigned long WIFI_BLINK_INTERVAL = 500;  // ms


// ---------------- COLORS ----------------
#define COLOR_WHITE 0xFFFF
#define COLOR_GREEN 0x07E0
#define COLOR_ORANGE 0xFD20
#define COLOR_RED 0xF800
#define COLOR_BLACK 0x0000



// ---------------- LOG MESSAGES ----------------

// INFO (0–10)
const char* infoLogs[] = {
  "Booting system",         // 0
  "System ready",           // 1
  "Waiting for WiFi",       // 2
  "WiFi connecting",        // 3
  "Time sync started",      // 4
  "Fetching config",        // 5
  "Using cached config",    // 6
  "Inside working hours",   // 7
  "Outside working hours",  // 8
  "Preparing sleep",        // 9
  "Waking from deep sleep"  // 10
};

// SUCCESS (11–19)
const char* successLogs[] = {
  "WiFi connected",       // 11
  "Time synchronized",    // 12
  "Config fetched",       // 13
  "Config applied",       // 14
  "Sensor read OK",       // 15
  "Data uploaded",        // 16
  "Upload acknowledged",  // 17
  "RTC time valid",       // 18
  "Sleep scheduled"       // 19
};

// WARNING (20–28)
const char* warningLogs[] = {
  "WiFi not connected",                  // 20
  "Weak WiFi signal",                    // 21
  "Config fetch skipped",                // 22
  "Using default config",                // 23
  "Sensor read delayed",                 // 24
  "Upload skipped: WiFi not connected",  // 25
  "Retrying upload",                     // 26
  "Low voltage detected",                // 27
  "Time not yet valid"                   // 28
};

// ERROR (29–36)
const char* errorLogs[] = {
  "WiFi connection failed",   // 29
  "Time sync failed",         // 30
  "Config fetch failed",      // 31
  "JSON parse error",         // 32
  "Upload failed",            // 33
  "HTTP error",               // 34
  "Invalid config received",  // 35
  "RTC data corrupted"        // 36
};

// ---------------- SLEEP VARIABLES ----------------
int zOffset = 0;
unsigned long lastZUpdate = 0;
const int zSpeed = 120;  // ms between frames
bool inSleepState = false;
bool isSleepUIDrawn = false;
bool isActiveUIDrawn = false;

// ---------------- DISPLAY OBJECT ----------------

MCUFRIEND_kbv tft;

// ---------------- TIMING ----------------
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 1000;  // 1 second


//---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);
  uint16_t ID = tft.readID();
  tft.begin(ID);

  tft.setRotation(2);      // Portrait (320x480)
  tft.fillScreen(0x0000);  // Black background
  initSensorValues();
  draw();  // Initial full UI draw
}


//--------------- LOOP ----------------
void loop() {
  if (!inSleepState) {
    if (!isActiveUIDrawn) {
      draw();
    }
    activeState();

  } else {
    if (!isSleepUIDrawn) {
      drawSleep();
      readSerialPacket();
      handlePacket();
    } else {
      animateSleepZ();
      setTimeTFT();
      readSerialPacket();
      handlePacket();
    }
  }


  // if (millis() - lastUpdate >= UPDATE_INTERVAL) {
  //   lastUpdate = millis();

  //   // fakeSensorUpdate();  // Update fake sensor values
  //   drawUpdate();        // Update numbers, bars & logs
  // }
}

void activeState() {
  isSleepUIDrawn = false;
  setTimeTFT();
  readSerialPacket();
  handlePacket();
  updateWiFiIcon();
}

//--------------- SENSOR SIMULATION ----------------
// void fakeSensorUpdate() {
//   // Random drift
//   nitrogen += random(-2, 3);
//   phosphorus += random(-2, 3);
//   potassium += random(-2, 3);

//   temperature += random(-5, 6) * 0.1;
//   moisture += random(-2, 3);
//   ph += random(-2, 3) * 0.05;

//   // Clamp realistic ranges
//   nitrogen = constrain(nitrogen, 0, 200);
//   phosphorus = constrain(phosphorus, 0, 200);
//   potassium = constrain(potassium, 0, 200);

//   temperature = constrain(temperature, 10, 50);
//   moisture = constrain(moisture, 0, 100);
//   ph = constrain(ph, 4.0, 9.0);

//   // Map to bar sizes
//   nBar = map(nitrogen, 0, 200, 0, 57);
//   pBar = map(phosphorus, 0, 200, 0, 57);
//   kBar = map(potassium, 0, 200, 0, 57);

//   tempBar = map(temperature, 10, 50, 0, 72);
//   phBar = map(ph * 10, 40, 90, 0, 72);
// }

//--------------- INITIALIZE SENSOR VALUES ----------------
void initSensorValues() {
  SENSOR_DATA.nitrogen = 145;
  SENSOR_DATA.phosphorus = 68;
  SENSOR_DATA.potassium = 110;
  SENSOR_DATA.temperature = 29.7;
  SENSOR_DATA.moisture = 76.0;
  SENSOR_DATA.pH = 6.8;
  SENSOR_DATA.conductivity = 0.0;
}



//--------------- HANDLE SENSOR PACKET ----------------
void handleSensorUpdate(const SensorData& data) {
  // Clamp NPK
  SENSOR_DATA.nitrogen = constrain(data.nitrogen, 0, 200);
  SENSOR_DATA.phosphorus = constrain(data.phosphorus, 0, 200);
  SENSOR_DATA.potassium = constrain(data.potassium, 0, 200);

  // Clamp environment values
  SENSOR_DATA.temperature = constrain(data.temperature, 10.0, 50.0);
  SENSOR_DATA.moisture = constrain(data.moisture, 0.0, 100.0);
  SENSOR_DATA.pH = constrain(data.pH, 0.0, 14.0);

  // Conductivity (usually uS/cm)
  SENSOR_DATA.conductivity = constrain(data.conductivity, 0.0, 5000.0);

  // Map to bars
  nBar = map(SENSOR_DATA.nitrogen, 0, 200, 0, 57);
  pBar = map(SENSOR_DATA.phosphorus, 0, 200, 0, 57);
  kBar = map(SENSOR_DATA.potassium, 0, 200, 0, 57);

  tempBar = map(SENSOR_DATA.temperature, 0, 45, 0, 72);
  phBar = map(SENSOR_DATA.pH * 10, 0, 140, 0, 72);
}


//--------------- FULL UI DRAW ----------------

static const unsigned char PROGMEM image_menu_settings_gear_bits[] = { 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x03, 0x0c, 0x30, 0xc0, 0x03, 0x0c, 0x30, 0xc0, 0x0c, 0xf0, 0x0f, 0x30, 0x0c, 0xf0, 0x0f, 0x30, 0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c, 0x0c, 0x0f, 0xf0, 0x30, 0x0c, 0x0f, 0xf0, 0x30, 0x0c, 0x30, 0x0c, 0x30, 0x0c, 0x30, 0x0c, 0x30, 0xf0, 0xc0, 0x03, 0x0f, 0xf0, 0xc0, 0x03, 0x0f, 0xc0, 0xc0, 0x03, 0x03, 0xc0, 0xc0, 0x03, 0x03, 0xc0, 0xc0, 0x03, 0x03, 0xc0, 0xc0, 0x03, 0x03, 0xf0, 0xc0, 0x03, 0x0f, 0xf0, 0xc0, 0x03, 0x0f, 0x0c, 0x30, 0x0c, 0x30, 0x0c, 0x30, 0x0c, 0x30, 0x0c, 0x0f, 0xf0, 0x30, 0x0c, 0x0f, 0xf0, 0x30, 0x30, 0x00, 0x00, 0x0c, 0x30, 0x00, 0x00, 0x0c, 0x0c, 0xf0, 0x0f, 0x30, 0x0c, 0xf0, 0x0f, 0x30, 0x03, 0x0c, 0x30, 0xc0, 0x03, 0x0c, 0x30, 0xc0, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x0f, 0xf0, 0x00 };

static const unsigned char PROGMEM image_usb_cable_connected_bits[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x03, 0xe0, 0x04, 0xc0, 0x08, 0x04, 0xc8, 0x06, 0xff, 0xff, 0xc2, 0x06, 0x02, 0x04, 0x01, 0x30, 0x00, 0xf8, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00 };

static const unsigned char PROGMEM image_wifi_bits[] = { 0x01, 0xf0, 0x00, 0x06, 0x0c, 0x00, 0x18, 0x03, 0x00, 0x21, 0xf0, 0x80, 0x46, 0x0c, 0x40, 0x88, 0x02, 0x20, 0x10, 0xe1, 0x00, 0x23, 0x18, 0x80, 0x04, 0x04, 0x00, 0x08, 0x42, 0x00, 0x01, 0xb0, 0x00, 0x02, 0x08, 0x00, 0x00, 0x40, 0x00, 0x00, 0xa0, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00 };

static const unsigned char PROGMEM image_plant_bits[] = { 0x00, 0x03, 0x00, 0x0f, 0x20, 0x1f, 0x70, 0x3f, 0xf8, 0x3f, 0xfc, 0x7e, 0xfc, 0x7e, 0xfc, 0x7c, 0x78, 0xf0, 0x30, 0xc0, 0x10, 0x80, 0x08, 0x80, 0x05, 0x00, 0x03, 0x00, 0x01, 0x00, 0x01, 0x00 };

static const unsigned char PROGMEM image_weather_temperature_bits[] = { 0x1c, 0x00, 0x22, 0x02, 0x2b, 0x05, 0x2a, 0x02, 0x2b, 0x38, 0x2a, 0x60, 0x2b, 0x40, 0x2a, 0x40, 0x2a, 0x60, 0x49, 0x38, 0x9c, 0x80, 0xae, 0x80, 0xbe, 0x80, 0x9c, 0x80, 0x41, 0x00, 0x3e, 0x00 };

static const unsigned char PROGMEM image_FaceNormal_bits[] = { 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x01, 0xe0, 0x7a, 0x00, 0x03, 0xd0, 0x7e, 0x00, 0x03, 0xf0, 0x7e, 0x00, 0x03, 0xf0, 0x7e, 0x00, 0x03, 0xf0, 0x3c, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x40, 0x00, 0x00, 0x10, 0x40, 0x00, 0x00, 0x10, 0x40, 0x00, 0x00, 0x08, 0x80, 0x00, 0x00, 0x07, 0x00, 0x00 };

static const unsigned char PROGMEM image_BLE_beacon_bits[] = { 0x44, 0x92, 0xaa, 0x92, 0x54, 0x10, 0x10, 0x7c };


//--------------- SLEEP UI DRAW ----------------
void drawSleep(void) {
  tft.fillScreen(0x0);

  // Layer 1
  tft.drawRect(10, 10, 300, 457, 0xFFFF);

  // Layer 3
  tft.setTextColor(0xFFFF);
  tft.setTextSize(4);
  tft.setTextWrap(false);
  tft.setCursor(39, 65);
  tft.println("--:--");

  // Layer 4
  tft.setTextColor(0x37A9);
  tft.setTextSize(2);
  tft.setCursor(44, 106);
  tft.println("--.--.---");

  // Layer 5
  tft.setTextColor(0xFFFF);
  tft.setTextSize(3);
  tft.setCursor(125, 153);
  tft.println("Deep Sleep ");

  // Layer 5
  tft.fillCircle(107, 293, 30, 0xFFFF);

  // Layer 5 copy
  tft.fillCircle(213, 291, 30, 0xFFFF);

  // Layer 5 copy
  tft.fillCircle(107, 280, 30, 0x0);

  // Layer 5 copy
  tft.fillCircle(213, 278, 30, 0x0);

  // Layer 9
  tft.setTextSize(3);
  tft.setFont();
  tft.setCursor(202, 239);
  tft.println("Z");

  // Layer 10
  tft.setTextSize(2);
  tft.setCursor(227, 219);
  tft.println("Z");

  // Layer 11
  tft.setTextSize(1);
  tft.setCursor(247, 205);
  tft.println("Z");

  // FaceNormal
  tft.drawBitmap(75, 429, image_FaceNormal_bits, 29, 14, 0xFFFF);

  //Personal touch
  tft.setTextSize(1);
  tft.setTextColor(0x37A9);
  tft.setCursor(118, 432);
  tft.println("Engineered by Tanishq");

  isSleepUIDrawn = true;
}

//--------------- ANIMATE SLEEP ----------------
void animateSleepZ() {
  if (millis() - lastZUpdate < zSpeed) return;
  lastZUpdate = millis();

  // Clear previous Zs (black color)
  tft.setTextColor(0x0000);
  tft.setTextSize(3);
  tft.setCursor(202, 239 - zOffset);
  tft.print("Z");

  tft.setTextSize(2);
  tft.setCursor(227, 219 - zOffset);
  tft.print("Z");

  tft.setTextSize(1);
  tft.setCursor(247, 205 - zOffset);
  tft.print("Z");

  // Update offset
  zOffset += 3;
  if (zOffset > 40) zOffset = 0;

  // Draw new Zs
  tft.setTextColor(0xFFFF);
  tft.setTextSize(3);
  tft.setCursor(202, 239 - zOffset);
  tft.print("Z");

  tft.setTextSize(2);
  tft.setCursor(227, 219 - zOffset);
  tft.print("Z");

  tft.setTextSize(1);
  tft.setCursor(247, 205 - zOffset);
  tft.print("Z");
}


//--------------- FULL UI DRAW ----------------
void draw() {
  tft.fillScreen(0x0000);

  // Background Frame
  tft.drawRect(10, 10, 300, 457, 0xFFFF);

  // Menu Bar separator
  tft.drawLine(11, 60, 309, 60, 0xFFFF);

  // WiFi icon
  drawWiFiIcon(COLOR_WHITE);
  // USB icon
  tft.drawBitmap(280, 27, image_usb_cable_connected_bits, 16, 16, 0xFFFF);

  // Date
  tft.setTextColor(0xFE00, 0x0000);
  tft.setTextSize(1);
  tft.setCursor(20, 45);
  tft.print("23.12.2025");

  // Time
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setTextSize(2);
  tft.setCursor(20, 22);
  tft.print("15:23");

  // Title
  tft.setTextSize(2);
  tft.setCursor(108, 28);
  tft.print("NPK Sensor");
  tft.setTextSize(1);

  // Serial box
  tft.drawRect(20, 355, 280, 60, 0xFF47);

  tft.setTextColor(0xFF47, 0x0000);
  tft.setCursor(28, 363);
  tft.print("Serial logs :");

  tft.setTextColor(0xFFFF, 0x0000);
  tft.setCursor(29, 373);
  tft.print("Sample serial data incoming");


  // BLE_beacon
  tft.drawBitmap(235, 320, image_BLE_beacon_bits, 7, 8, 0xFFFF);

  // FaceNormal
  tft.drawBitmap(75, 429, image_FaceNormal_bits, 29, 14, 0xFFFF);


  //Personal touch
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);
  tft.setCursor(118, 432);
  tft.println("Engineered by Tanishq");



  // ========= NPK SECTION =========

  tft.drawRect(21, 71, 81, 105, 0xFFFF);
  tft.drawRect(120, 71, 81, 105, 0xFFFF);
  tft.drawRect(217, 72, 81, 105, 0xFFFF);

  // Nitrogen
  tft.setTextColor(0xEC5F, 0x0000);
  tft.setCursor(37, 85);
  tft.print("Nitrogen");

  tft.setTextSize(2);
  tft.setCursor(43, 110);
  tft.print("...");

  tft.drawRect(31, 152, 59, 10, 0xEC5F);
  tft.fillRect(32, 153, 45, 8, 0xD85F);

  // Phosphorus
  tft.setTextSize(1);
  tft.setTextColor(0xFE00, 0x0000);
  tft.setCursor(129, 85);
  tft.print("Phosphorus");

  tft.setTextSize(2);
  tft.setCursor(147, 110);
  tft.print("...");

  tft.drawRect(131, 152, 59, 10, 0xFE00);
  tft.fillRect(132, 153, 45, 8, 0xEFE0);

  // Potassium
  tft.setTextSize(1);
  tft.setTextColor(0xFBC0, 0x0000);
  tft.setCursor(232, 85);
  tft.print("Potassium");

  tft.setTextSize(2);
  tft.setCursor(242, 109);
  tft.print("...");

  tft.drawRect(229, 152, 59, 10, 0xF300);
  tft.fillRect(230, 153, 45, 8, 0xFBC0);

  // ========= LOWER SECTION =========

  tft.drawRect(21, 190, 81, 105, 0xFFFF);
  tft.drawRect(120, 191, 81, 105, 0xFFFF);
  tft.drawRect(217, 191, 81, 105, 0xFFFF);

  // Temperature
  tft.setTextSize(1);
  tft.setTextColor(0xE8EC, 0x0000);
  tft.setCursor(57, 211);
  tft.print("Temp.");

  tft.setCursor(57, 230);
  tft.print("...");
  tft.print(" C");

  tft.drawRect(31, 211, 12, 72, 0xE8EC);
  tft.fillRect(32, 244, 10, 38, 0xE8EC);

  // Temp
  tft.drawBitmap(65, 255, image_weather_temperature_bits, 16, 16, 0xE8EC);

  // Moisture
  tft.setTextColor(0x05FA, 0x0000);
  tft.setCursor(137, 211);
  tft.print("Moisture");

  tft.fillCircle(160, 251, 26, 0x055E);
  tft.fillCircle(160, 251, 19, 0x0000);

  tft.setTextSize(1);
  tft.setCursor(154, 251);
  tft.print("...");

  // pH
  tft.setTextSize(1);
  tft.setTextColor(0x06B5, 0x0000);
  tft.setCursor(246, 211);
  tft.print("Ph");

  tft.setCursor(246, 230);
  tft.print("...");

  // plant
  tft.drawBitmap(244, 255, image_plant_bits, 16, 16, 0x06B5);


  tft.drawRect(275, 210, 12, 72, 0x04B1);
  tft.fillRect(276, 243, 10, 38, 0x0798);

  // Label conductivity

  // Layer 44
  tft.drawRect(21, 305, 274, 38, 0xFFFF);

  tft.setTextColor(0xE8F);
  tft.setCursor(70, 322);
  tft.println("Conductivity");

  // conductivity
  tft.setCursor(150, 321);
  tft.println("....");
  tft.setCursor(182, 321);
  tft.println(" uS/cm");

  isActiveUIDrawn = true;
}

//--------------- DYNAMIC UPDATE ----------------

void drawUpdate() {
  drawValues();     // Update numeric values
  drawBarUpdate();  // Update bar visuals
  //updateSerialLog();  // Update log message
}

//--------------- VALUE UPDATE ----------------


void drawValues() {

  // ---------- NITROGEN ----------
  CLEAR_TEXT(40, 105, 50, 20);
  tft.setTextColor(0xEC5F, COLOR_BLACK);
  tft.setTextSize(2);
  tft.setCursor(43, 110);
  tft.print(SENSOR_DATA.nitrogen);

  // ---------- PHOSPHORUS ----------
  CLEAR_TEXT(145, 105, 50, 20);
  tft.setTextColor(0xFE00, COLOR_BLACK);
  tft.setCursor(147, 110);
  tft.print(SENSOR_DATA.phosphorus);

  // ---------- POTASSIUM ----------
  CLEAR_TEXT(240, 105, 50, 20);
  tft.setTextColor(0xFBC0, COLOR_BLACK);
  tft.setCursor(242, 109);
  tft.print(SENSOR_DATA.potassium);

  // ---------- TEMPERATURE ----------
  CLEAR_TEXT(55, 225, 30, 15);
  tft.setTextSize(1);
  tft.setTextColor(0xE8EC, COLOR_BLACK);
  tft.setCursor(57, 230);
  tft.print(SENSOR_DATA.temperature, 1);
  tft.print(" C");

  // ---------- MOISTURE ----------
  CLEAR_TEXT(145, 245, 30, 15);
  tft.setTextColor(0x05FA, COLOR_BLACK);
  tft.setCursor(154, 251);
  tft.print(SENSOR_DATA.moisture, 0);

  // ---------- pH ----------
  CLEAR_TEXT(240, 225, 30, 15);
  tft.setTextColor(0x06B5, COLOR_BLACK);
  tft.setCursor(246, 230);
  tft.print(SENSOR_DATA.pH, 1);

  // ---------- CONDUCTIVITY ----------
  CLEAR_TEXT(145, 318, 40, 14);
  tft.setTextColor(getConductivityColor(SENSOR_DATA.conductivity), COLOR_BLACK);
  tft.setCursor(155, 321);

  if (SENSOR_DATA.conductivity < 1000) tft.print("0");
  if (SENSOR_DATA.conductivity < 100) tft.print("0");
  if (SENSOR_DATA.conductivity < 10) tft.print("0");

  tft.print((int)SENSOR_DATA.conductivity);
}

/*
void drawValues() {
  // Nitrogen
  tft.setTextColor(0xEC5F, 0x0000);
  tft.setTextSize(2);
  tft.setCursor(43, 110);
  tft.print(SENSOR_DATA.nitrogen);

  // Phosphorus
  tft.setTextColor(0xFE00, 0x0000);
  tft.setCursor(147, 110);
  tft.print(SENSOR_DATA.phosphorus);

  // Potassium
  tft.setTextColor(0xFBC0, 0x0000);
  tft.setCursor(242, 109);
  tft.print(SENSOR_DATA.potassium);

  // Temperature
  tft.setTextSize(1);
  tft.setTextColor(0xE8EC, 0x0000);
  tft.setCursor(57, 230);
  tft.print(SENSOR_DATA.temperature, 1);
  tft.print(" C");

  // Moisture
  tft.setTextColor(0x05FA, 0x0000);
  tft.setCursor(151, 251);
  tft.print(SENSOR_DATA.moisture);

  // pH
  tft.setTextColor(0x06B5, 0x0000);
  tft.setCursor(244, 230);
  tft.print(SENSOR_DATA.pH, 1);

  // Conductivity
  tft.fillRect(145, 320, 30, 10, 0x0000);
  tft.setTextColor(getConductivityColor(SENSOR_DATA.conductivity), COLOR_BLACK);
  tft.setCursor(150, 321);

  // Fixed width: 4 digits
  if (SENSOR_DATA.conductivity < 1000) tft.print("0");
  if (SENSOR_DATA.conductivity < 100) tft.print("0");
  if (SENSOR_DATA.conductivity < 10) tft.print("0");

  tft.print(SENSOR_DATA.conductivity);
}
*/

//--------------- BAR UPDATE ----------------

void drawBarUpdate() {
  // Clear bars
  tft.fillRect(32, 153, 57, 8, 0x0000);
  tft.fillRect(132, 153, 57, 8, 0x0000);
  tft.fillRect(230, 153, 57, 8, 0x0000);

  tft.fillRect(32, 212, 10, 70, 0x0000);
  tft.fillRect(276, 212, 10, 70, 0x0000);

  // Draw bars
  tft.fillRect(32, 153, nBar, 8, 0xD85F);
  tft.fillRect(132, 153, pBar, 8, 0xEFE0);
  tft.fillRect(230, 153, kBar, 8, 0xFBC0);

  tft.fillRect(32, 211 + (72 - tempBar), 10, tempBar, 0xE8EC);
  tft.fillRect(276, 210 + (72 - phBar), 10, phBar, 0x0798);
}


//--------------- SERIAL LOG UPDATE ----------------

// void updateSerialLog() {
//   tft.fillRect(26, 328, 269, 44, 0x0000);

//   int type = random(0, 3);
//   const char* msg;
//   uint16_t color;

//   if (type == LOG_SUCCESS) {
//     msg = successLogs[random(0, 5)];
//     color = 0x07E0;  // Green
//   } else if (type == LOG_WARNING) {
//     msg = warningLogs[random(0, 5)];
//     color = 0xF800;  // Red
//   } else {
//     msg = infoLogs[random(0, 5)];
//     color = 0xFFFF;  // White
//   }

//   tft.setTextColor(color, 0x0000);
//   tft.setCursor(29, 333);
//   tft.print(msg);
// }

void showLogByID(uint8_t id) {
  const char* msg = "";
  uint16_t color = 0xFFFF;

  if (id < 11) {
    if (id < sizeof(infoLogs) / sizeof(infoLogs[0]))
      msg = infoLogs[id];
    color = 0xFFFF;  // White
  } else if (id < 20) {
    uint8_t i = id - 11;
    if (i < sizeof(successLogs) / sizeof(successLogs[0]))
      msg = successLogs[i];
    color = 0x07E0;  // Green
  } else if (id < 29) {
    uint8_t i = id - 20;
    if (i < sizeof(warningLogs) / sizeof(warningLogs[0]))
      msg = warningLogs[i];
    color = 0xFD20;
  } else {
    uint8_t i = id - 29;
    if (i < sizeof(errorLogs) / sizeof(errorLogs[0]))
      msg = errorLogs[i];
    color = 0xF800;  // Red
  }

  tft.fillRect(26, 371, 269, 43, 0x0000);
  tft.setTextColor(color, 0x0000);
  tft.setCursor(29, 373);
  tft.print(msg);
}

//--------------- READ SERIAL PACKET ----------------

void readSerialPacket() {
  static uint8_t index = 0;

  while (Serial.available()) {
    char c = Serial.read();

    if (c == '$') {
      index = 0;
      serialBuffer[index++] = c;
    } else if (index > 0 && index < sizeof(serialBuffer) - 1) {
      serialBuffer[index++] = c;

      if (c == '#') {
        serialBuffer[index] = '\0';
        packetReady = true;
        index = 0;
      }
    }
  }
}

//--------------- PACKET DISPATCHER ----------------

void handlePacket() {
  if (!packetReady) return;
  packetReady = false;

  // SENSOR DATA
  if (strncmp(serialBuffer, "$SENS", 5) == 0) {
    parseSensorPacket();
  }
  // LOG EVENT
  else if (strncmp(serialBuffer, "$LOG", 4) == 0) {
    parseLogPacket();
  }
  // TIME
  else if (strncmp(serialBuffer, "$TIME", 5) == 0) {
    parseTimePacket();
  }
}


//--------------- SENSOR PACKET PARSER ----------------
// $SENS,n,ph,po,temp,moi,pH,conductivity#


void parseSensorPacket() {

  char* ptr = strtok(serialBuffer, ",");

  ptr = strtok(NULL, ",");
  SENSOR_DATA.nitrogen = atoi(ptr);
  ptr = strtok(NULL, ",");
  SENSOR_DATA.phosphorus = atoi(ptr);
  ptr = strtok(NULL, ",");
  SENSOR_DATA.potassium = atoi(ptr);
  ptr = strtok(NULL, ",");
  SENSOR_DATA.temperature = atof(ptr);
  ptr = strtok(NULL, ",");
  SENSOR_DATA.moisture = atof(ptr);
  ptr = strtok(NULL, ",");
  SENSOR_DATA.pH = atof(ptr);
  ptr = strtok(NULL, ",");
  SENSOR_DATA.conductivity = atof(ptr);


  handleSensorUpdate(SENSOR_DATA);
  drawUpdate();  //update values and bars
}

//--------------- LOG PACKET PARSER ----------------
// $LOG,10#

void parseLogPacket() {
  uint8_t logID = 0;
  sscanf(serialBuffer, "$LOG,%hhu", &logID);
  Serial.print(logID);
  Serial.println("");
  showLogByID(logID);
  handleLogID(logID);
}

//--------------- TIME PACKET PARSER ----------------
// $TIME,<epoch>#

void parseTimePacket() {
  unsigned long epoch = 0;
  sscanf(serialBuffer, "$TIME,%lu", &epoch);

  baseEpoch = epoch;
  baseMillis = millis();
  timeValid = true;
}

//--------------- GET CURRENT EPOCH ----------------
unsigned long getCurrentEpoch() {
  if (!timeValid) return 0;
  return baseEpoch + (millis() - baseMillis) / 1000;
}

//--------------- GET TIME ----------------
void getTimeHM(uint8_t& hh, uint8_t& mm) {
  unsigned long epoch = getCurrentEpoch();
  epoch += IST_OFFSET;  // apply timezone manually

  epoch %= 86400;  // seconds today
  hh = epoch / 3600;
  mm = (epoch % 3600) / 60;
}



//--------------- LEAP YEAR HELPER FUNC ----------------
bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

//--------------- GET DATE ----------------
void getDateYMD(uint16_t& year, uint8_t& month, uint8_t& day) {
  unsigned long t = getCurrentEpoch();
  t += IST_OFFSET;  // apply timezone manually
  if (t == 0) {
    year = 0;
    month = 0;
    day = 0;
    return;
  }

  // Days since 1970-01-01
  unsigned long days = t / 86400;

  year = 1970;

  // Find current year
  while (true) {
    unsigned int daysInYear = isLeapYear(year) ? 366 : 365;
    if (days >= daysInYear) {
      days -= daysInYear;
      year++;
    } else {
      break;
    }
  }

  // Month lengths
  const uint8_t monthDays[12] = {
    31, 28, 31, 30, 31, 30,
    31, 31, 30, 31, 30, 31
  };

  month = 0;

  // Find current month
  for (uint8_t i = 0; i < 12; i++) {
    uint8_t dim = monthDays[i];
    if (i == 1 && isLeapYear(year)) dim = 29;  // February leap year

    if (days >= dim) {
      days -= dim;
      month++;
    } else {
      break;
    }
  }

  day = days + 1;  // Day starts from 1
  month += 1;      // Month starts from 1
}

//--------------- SET TIME ON TFT ----------------

void setTimeTFT() {
  getTimeHM(hh, mm);
  getDateYMD(yy, mo, dd);

if(!inSleepState){
  tft.setTextSize(2);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setCursor(20, 22);
}
else{
  tft.setTextColor(0xFFFF,0x0000);
  tft.setTextSize(4);
  tft.setCursor(39, 65);
}


  if (timeValid) {
    if (hh < 10) tft.print("0");
    tft.print(hh);
    tft.print(":");
    if (mm < 10) tft.print("0");
    tft.print(mm);
  } else {
    tft.print("--:--");
  }

  // Date
  if(!inSleepState){
  tft.setTextSize(1);
  tft.setCursor(20, 45);
  tft.setTextColor(0xFE00, 0x0000);

}
else{
  tft.setTextColor(0x37A9,0x0000);
  tft.setTextSize(2);
  tft.setCursor(44, 106);
}

  if (timeValid) {
    if (dd < 10) tft.print("0");
    tft.print(dd);
    tft.print(".");

    if (mo < 10) tft.print("0");
    tft.print(mo);
    tft.print(".");

    tft.print(yy);
  } else {
    tft.print("--.--.----");
  }
}



//--------------- DRAW WIFI ICON ----------------

void drawWiFiIcon(uint16_t color) {
  // Optional: clear area to avoid ghosting
  tft.fillRect(250, 28, 19, 16, COLOR_BLACK);
  tft.drawBitmap(250, 28, image_wifi_bits, 19, 16, color);
}

//--------------- UPDATE WIFI ICON ----------------
void updateWiFiIcon() {
  unsigned long now = millis();
  uint16_t colorToDraw = lastWifiColor;
  bool shouldDraw = false;

  // State changed
  if (wifiState != lastWifiState) {
    shouldDraw = true;
    wifiBlinkOn = false;  // reset blink
    wifiBlinkTimer = now;
    lastWifiState = wifiState;
  }

  switch (wifiState) {

    case WIFI_CONNECTING:
      if (now - wifiBlinkTimer >= WIFI_BLINK_INTERVAL) {
        wifiBlinkTimer = now;
        wifiBlinkOn = !wifiBlinkOn;
        colorToDraw = wifiBlinkOn ? COLOR_ORANGE : COLOR_BLACK;
        shouldDraw = true;
      }
      break;

    case WIFI_CONNECTED:
      colorToDraw = COLOR_GREEN;
      break;

    case WIFI_FAILED:
      colorToDraw = COLOR_RED;
      break;

    default:
      colorToDraw = COLOR_WHITE;
      break;
  }

  // Draw only if color actually changed
  if (shouldDraw && colorToDraw != lastWifiColor) {
    drawWiFiIcon(colorToDraw);
    lastWifiColor = colorToDraw;
  }
}



//--------------- HANDLE LOGID ----------------
void handleLogID(int logID) {
  switch (logID) {
    case 0:  //Booting system
      inSleepState = false;
      isActiveUIDrawn = false;
      wifiState = WIFI_IDLE;
      break;

    case 2:  // Waiting for WiFi
    case 3:  // WiFi connecting
      wifiState = WIFI_CONNECTING;
      break;

    case 11:  // WiFi connected
      wifiState = WIFI_CONNECTED;
      break;

    case 19:  // Sleep Scheduled
      inSleepState = true;
      isSleepUIDrawn = false;
      isActiveUIDrawn = false;
      wifiState = WIFI_IDLE;
      break;
    case 20:  // WiFi not connected
    case 29:  // WiFi connection failed
      wifiState = WIFI_FAILED;
      break;
  }
}

//--------------- CONDUCTIVITY UTIL ----------------
uint16_t getConductivityColor(float ec) {
  if (ec < 500) return COLOR_ORANGE;  // low nutrients
  if (ec < 2000) return COLOR_GREEN;  // ideal
  return COLOR_RED;                   // too high
}
