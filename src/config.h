#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Display modes, in rotation order.
// ---------------------------------------------------------------------------
enum DisplayMode : uint8_t {
  MODE_BIG_TIME = 0,       // Large time, blinking colon
  MODE_SMALL_TIME_SEC,     // Small time with seconds
  MODE_TIME_DATE,          // Time + date
  MODE_TIME_DATE_WEATHER,  // Time + date + weather
  MODE_TIME_WEATHER,       // Time + weather
  MODE_SYSTEM_INFO,        // Firmware version/build + uptime
  MODE_COUNT
};

// Manual screen-power override, driven by the Web UI (touch always wins
// over this and returns the display to AUTO — see display.cpp).
enum ManualScreenState : uint8_t {
  SCREEN_AUTO = 0,
  SCREEN_FORCED_ON,
  SCREEN_FORCED_OFF
};

// ---------------------------------------------------------------------------
// Persistent settings. Everything here survives reboot via NVS (Preferences)
// and is restored to these defaults on factory reset.
// ---------------------------------------------------------------------------
struct Settings {
  // Wi-Fi
  char wifiSSID[33] = "";
  char wifiPass[65] = "";

  // Time
  bool use24Hour = false;
  uint8_t tzIndex = 0; // index into TIMEZONES[] in time_manager.h

  // Weather
  char zipCode[8]      = "48842";
  uint16_t weatherIntervalMin = 15;
  bool useCelsius       = false;
  float cachedLat       = 0.0f;
  float cachedLon       = 0.0f;
  bool hasCoords        = false;
  char coordsForZip[8]  = ""; // zip that cachedLat/cachedLon were resolved for

  // Display / rotation
  bool autoRotate           = true;
  uint16_t rotateIntervalSec = 8;
  uint8_t brightness         = 200; // 0-255, sent to SSD1306 contrast register

  // Screen schedule (24h clock, minute resolution)
  bool scheduleEnabled  = false;
  uint8_t scheduleOffHour   = 23;
  uint8_t scheduleOffMinute = 0;
  uint8_t scheduleOnHour    = 7;
  uint8_t scheduleOnMinute  = 0;

  // Firmware update auth (placeholder default — change it after flashing!)
  char otaPassword[17] = "1234";
};

extern Settings settings;

void configLoad();          // load from NVS, filling in defaults for missing keys
void configSave();          // persist current `settings` to NVS
void configFactoryReset();  // wipe NVS namespace, reset `settings` to defaults, save
