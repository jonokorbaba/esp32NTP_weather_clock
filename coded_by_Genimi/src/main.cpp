#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "time_mgr.h"
#include "weather_mgr.h"
#include "display_mgr.h"
#include "web_server.h"

SystemConfig sysCfg;
Preferences prefs;

// TTP223 Touch Interrupt Handler
static volatile bool touchDetected = false;
static unsigned long lastTouchTime = 0;

void IRAM_ATTR handleTouchISR() {
  touchDetected = true;
}

void loadConfiguration() {
  // Try opening in read-only mode first
  if (!prefs.begin("clock_cfg", true)) {
    Serial.println("[NVS] Configuration namespace not found. Initializing defaults...");
    prefs.end();
    
    // Open in read-write mode to force creation of namespace and save defaults
    saveConfiguration();
    return;
  }

  // Load existing values if namespace was found
  sysCfg.weatherRefreshMin = prefs.getUInt("wref", 30);
  sysCfg.tempUnitF = prefs.getBool("tempF", true);
  sysCfg.is12Hour = prefs.getBool("is12h", true);
  sysCfg.autoRotate = prefs.getBool("rotOn", false);
  sysCfg.rotateIntervalSec = prefs.getUInt("rotSec", 10);
  sysCfg.oledBrightness = prefs.getUChar("bright", 255);
  sysCfg.schedScreenEnable = prefs.getBool("scEn", false);
  sysCfg.screenOnHour = prefs.getUChar("scOn", 7);
  sysCfg.screenOffHour = prefs.getUChar("scOff", 22);

  String zip = prefs.getString("zip", "48842");
  strncpy(sysCfg.zipCode, zip.c_str(), sizeof(sysCfg.zipCode));

  String tz = prefs.getString("tz", "EST5EDT,M3.2.0,M11.1.0");
  strncpy(sysCfg.posixTz, tz.c_str(), sizeof(sysCfg.posixTz));

  String pass = prefs.getString("pass", OTA_PASS_DEFAULT);
  strncpy(sysCfg.otaPassword, pass.c_str(), sizeof(sysCfg.otaPassword));
  
  prefs.end();
}

void saveConfiguration() {
  prefs.begin("clock_cfg", false);
  prefs.putUInt("wref", sysCfg.weatherRefreshMin);
  prefs.putBool("tempF", sysCfg.tempUnitF);
  prefs.putBool("is12h", sysCfg.is12Hour);
  prefs.putBool("rotOn", sysCfg.autoRotate);
  prefs.putUInt("rotSec", sysCfg.rotateIntervalSec);
  prefs.putUChar("bright", sysCfg.oledBrightness);
  prefs.putBool("scEn", sysCfg.schedScreenEnable);
  prefs.putUChar("scOn", sysCfg.screenOnHour);
  prefs.putUChar("scOff", sysCfg.screenOffHour);
  prefs.putString("zip", sysCfg.zipCode);
  prefs.putString("tz", sysCfg.posixTz);
  prefs.putString("pass", sysCfg.otaPassword);
  prefs.end();
}

void resetConfigurationToDefaults() {
  prefs.begin("clock_cfg", false);
  prefs.clear();
  prefs.end();
  loadConfiguration();
}

void setup() {
  Serial.begin(115200);

  loadConfiguration();
  pinMode(PIN_TOUCH, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_TOUCH), handleTouchISR, RISING);

  initDisplayManager();

  // Smart Wi-Fi Connection (Non-blocking fallback)
  WiFi.mode(WIFI_STA);
  WiFi.begin(); // Uses stored credentials if available

  initTimeManager();
  initWeatherManager();
  initWebServer();
}

void loop() {
  // Handle Touch Button with Debounce
  if (touchDetected) {
    touchDetected = false;
    if (millis() - lastTouchTime > 300) { // 300ms debounce
      lastTouchTime = millis();
      if (!isScreenOn()) {
        setScreenPower(true);
      } else {
        nextDisplayMode();
      }
    }
  }

  // Non-blocking Task Loops
  loopTimeManager();
  loopWeatherManager();
  loopDisplayManager();
}