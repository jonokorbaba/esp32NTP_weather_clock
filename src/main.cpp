#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "pins.h"
#include "time_manager.h"
#include "weather.h"
#include "display.h"
#include "webui.h"

const char* FW_VERSION = "1.0.0";
const char* FW_BUILD = __DATE__ " " __TIME__;
unsigned long bootMillis = 0;

// ---------------------------------------------------------------------------
// Wi-Fi connection state machine — entirely non-blocking. WiFi.begin() on
// ESP32 returns immediately; we just poll WiFi.status() from here on.
//
//   no credentials / repeated failure -> AP fallback (WIFI_AP_STA) so the
//   Web UI config portal is always reachable, while we keep retrying the
//   station connection in the background every RETRY_INTERVAL_MS.
// ---------------------------------------------------------------------------
enum WifiPhase { WIFI_CONNECTING, WIFI_CONNECTED, WIFI_AP_FALLBACK };
static WifiPhase wifiPhase = WIFI_CONNECTING;
static unsigned long wifiPhaseStartMillis = 0;
static unsigned long lastStaRetryMillis = 0;
static const unsigned long CONNECT_TIMEOUT_MS = 15000;
static const unsigned long RETRY_INTERVAL_MS = 30000;

static void startApFallback() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32Clock-Setup");
  wifiPhase = WIFI_AP_FALLBACK;
  wifiPhaseStartMillis = millis();
  Serial.println("[WiFi] AP fallback active: join 'ESP32Clock-Setup', browse http://192.168.4.1/");
}

static void beginStationConnect() {
  WiFi.begin(settings.wifiSSID, settings.wifiPass);
  wifiPhase = WIFI_CONNECTING;
  wifiPhaseStartMillis = millis();
}

void wifiApplyNewCredentials() {
  // Called by the Web UI right after new Wi-Fi credentials are saved.
  beginStationConnect();
}

static void wifiSetup() {
  WiFi.persistent(false); // we manage credentials ourselves via Preferences
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  if (strlen(settings.wifiSSID) > 0) {
    beginStationConnect();
  } else {
    startApFallback();
  }
}

static void wifiMaintain() {
  unsigned long now = millis();
  switch (wifiPhase) {
    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[WiFi] connected, IP=");
        Serial.println(WiFi.localIP());
        wifiPhase = WIFI_CONNECTED;
        timeOnWifiConnected();
      } else if (now - wifiPhaseStartMillis > CONNECT_TIMEOUT_MS) {
        startApFallback();
      }
      break;

    case WIFI_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] lost connection, retrying...");
        wifiPhase = WIFI_CONNECTING;
        wifiPhaseStartMillis = now;
      }
      break;

    case WIFI_AP_FALLBACK:
      if (strlen(settings.wifiSSID) > 0 && (now - lastStaRetryMillis > RETRY_INTERVAL_MS)) {
        lastStaRetryMillis = now;
        WiFi.begin(settings.wifiSSID, settings.wifiPass);
        wifiPhase = WIFI_CONNECTING;
        wifiPhaseStartMillis = now;
        // AP stays up concurrently (WIFI_AP_STA) so the config portal
        // remains reachable while we retry the station connection.
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  bootMillis = millis();

  configLoad();
  timeInit();
  displayInit();
  wifiSetup();
  webInit();
  weatherInit();
}

void loop() {
  wifiMaintain();
  timeMaybeResync();
  webHandleClient();
  displayHandleTouch();
  displayUpdate();
}
