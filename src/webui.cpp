#include "webui.h"
#include "config.h"
#include "time_manager.h"
#include "weather.h"
#include "display.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <string.h>

extern const char* FW_VERSION;
extern const char* FW_BUILD;
extern unsigned long bootMillis;
extern void wifiApplyNewCredentials(); // defined in main.cpp

static WebServer server(80);
static bool updateAuthFailed = false;

// ---------------------------------------------------------------------------
// Small HTML helpers — plain strings, no templating framework, per the
// project's "keep it simple" guidance.
// ---------------------------------------------------------------------------
static String htmlHeader(const String& title) {
  String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<title>" + title + "</title><style>";
  h += "body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 12px;background:#111;color:#eee}";
  h += "a{color:#7cf}fieldset{border:1px solid #444;border-radius:6px;margin-bottom:14px}";
  h += "legend{padding:0 6px}input,select{width:100%;box-sizing:border-box;padding:6px;margin:4px 0 10px;background:#222;color:#eee;border:1px solid #555;border-radius:4px}";
  h += "input[type=checkbox]{width:auto}label{display:block;margin-top:6px}";
  h += "button,input[type=submit]{background:#357;color:#fff;border:none;padding:10px 16px;border-radius:4px;cursor:pointer}";
  h += ".row{display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px solid #333}";
  h += ".nav a{margin-right:14px}";
  h += "</style></head><body>";
  h += "<h2>" + title + "</h2>";
  h += "<p class='nav'><a href='/'>Dashboard</a><a href='/settings'>Settings</a><a href='/wifi'>Wi-Fi</a><a href='/update'>Firmware</a></p>";
  return h;
}
static const char* htmlFooter = "</body></html>";

static void sendPage(const String& body, const String& title = "ESP32 Clock") {
  server.send(200, "text/html", htmlHeader(title) + body + htmlFooter);
}

static void redirectTo(const String& path) {
  server.sendHeader("Location", path, true);
  server.send(303, "text/plain", "");
}

// ---------------------------------------------------------------------------
// Dashboard
// ---------------------------------------------------------------------------
static String wifiStatusText() {
  if (WiFi.status() == WL_CONNECTED) {
    return "Connected to " + String(settings.wifiSSID) + " (" + WiFi.localIP().toString() + ")";
  }
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    return "Setup mode: join Wi-Fi \"ESP32Clock-Setup\", then browse to 192.168.4.1";
  }
  return "Connecting...";
}

static void handleRoot() {
  struct tm t;
  bool synced = timeGetLocal(&t);
  WeatherSnapshot w = weatherGetSnapshot();

  String body;
  body += "<div class='row'><span>Time</span><span>";
  if (synced) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    body += buf;
  } else body += "not synced yet";
  body += "</span></div>";

  body += "<div class='row'><span>Weather</span><span>";
  if (w.everSucceeded) {
    body += String(w.temperature, 0) + (settings.useCelsius ? "C " : "F ") + weatherConditionLabel(w.condition);
    if (w.stale) body += " (cached)";
  } else body += "not available yet";
  body += "</span></div>";

  if (w.lastSuccessEpoch > 0) {
    time_t now = time(nullptr);
    long ago = now - w.lastSuccessEpoch;
    body += "<div class='row'><span>Last weather update</span><span>" + String(ago / 60) + " min ago</span></div>";
  }
  if (w.nextAttemptEpoch > 0) {
    time_t now = time(nullptr);
    long remain = w.nextAttemptEpoch - now;
    if (remain < 0) remain = 0;
    body += "<div class='row'><span>Next weather sync</span><span>~" + String(remain / 60) + " min</span></div>";
  }
  if (strlen(w.lastError) > 0 && w.stale) {
    body += "<div class='row'><span>Last weather error</span><span>" + String(w.lastError) + "</span></div>";
  }

  body += "<div class='row'><span>Wi-Fi</span><span>" + wifiStatusText() + "</span></div>";
  body += "<div class='row'><span>Display mode</span><span>" + String(displayModeLabel(displayCurrentMode())) + "</span></div>";
  body += "<div class='row'><span>Screen</span><span>" + String(displayIsAwake() ? "On" : "Off") + "</span></div>";
  body += "<div class='row'><span>Firmware</span><span>" + String(FW_VERSION) + "</span></div>";
  body += "<div class='row'><span>Build</span><span>" + String(FW_BUILD) + "</span></div>";

  unsigned long s = (millis() - bootMillis) / 1000;
  body += "<div class='row'><span>Uptime</span><span>" + String(s / 3600) + "h " + String((s % 3600) / 60) + "m</span></div>";

  body += "<p><b>Screen control:</b> ";
  body += "<a href='/screen?state=on'>Turn On</a> | ";
  body += "<a href='/screen?state=off'>Turn Off</a> | ";
  body += "<a href='/screen?state=auto'>Auto (schedule)</a></p>";

  body += "<p><a href='/factory-reset'>Factory reset</a></p>";

  sendPage(body);
}

static void handleScreen() {
  String state = server.arg("state");
  if (state == "on") displaySetManualState(SCREEN_FORCED_ON);
  else if (state == "off") displaySetManualState(SCREEN_FORCED_OFF);
  else displaySetManualState(SCREEN_AUTO);
  redirectTo("/");
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
static String tzOptions() {
  String s;
  for (uint8_t i = 0; i < TIMEZONE_COUNT; i++) {
    s += "<option value='" + String(i) + "'";
    if (i == settings.tzIndex) s += " selected";
    s += ">" + String(TIMEZONES[i].label) + "</option>";
  }
  return s;
}

static void handleSettingsForm() {
  String body = "<form method='POST' action='/settings'>";

  body += "<fieldset><legend>Time</legend>";
  body += "<label>Timezone<select name='tz'>" + tzOptions() + "</select></label>";
  body += "<label><input type='checkbox' name='use24h'" + String(settings.use24Hour ? " checked" : "") + "> 24-hour format</label>";
  body += "</fieldset>";

  body += "<fieldset><legend>Weather</legend>";
  body += "<label>ZIP code<input name='zip' value='" + String(settings.zipCode) + "' maxlength='6'></label>";
  body += "<label>Refresh interval (minutes)<input type='number' name='wxint' min='1' max='1440' value='" + String(settings.weatherIntervalMin) + "'></label>";
  body += "<label><input type='checkbox' name='usec'" + String(settings.useCelsius ? " checked" : "") + "> Use Celsius</label>";
  body += "</fieldset>";

  body += "<fieldset><legend>Display</legend>";
  body += "<label><input type='checkbox' name='autorot'" + String(settings.autoRotate ? " checked" : "") + "> Auto-rotate modes</label>";
  body += "<label>Rotation interval (seconds)<input type='number' name='rotsec' min='2' max='120' value='" + String(settings.rotateIntervalSec) + "'></label>";
  body += "<label>Brightness (0-255)<input type='range' name='bright' min='1' max='255' value='" + String(settings.brightness) + "'></label>";
  body += "</fieldset>";

  body += "<fieldset><legend>Screen schedule</legend>";
  body += "<label><input type='checkbox' name='schen'" + String(settings.scheduleEnabled ? " checked" : "") + "> Enabled</label>";
  body += "<label>Off time (24h)<input type='number' name='schoffh' min='0' max='23' value='" + String(settings.scheduleOffHour) +
          "' style='width:45%;display:inline-block'> : <input type='number' name='schoffm' min='0' max='59' value='" + String(settings.scheduleOffMinute) + "' style='width:45%;display:inline-block'></label>";
  body += "<label>On time (24h)<input type='number' name='schonh' min='0' max='23' value='" + String(settings.scheduleOnHour) +
          "' style='width:45%;display:inline-block'> : <input type='number' name='schonm' min='0' max='59' value='" + String(settings.scheduleOnMinute) + "' style='width:45%;display:inline-block'></label>";
  body += "</fieldset>";

  body += "<fieldset><legend>Firmware update password</legend>";
  body += "<label>OTA password<input name='otapw' value='" + String(settings.otaPassword) + "'></label>";
  body += "</fieldset>";

  body += "<input type='submit' value='Save'></form>";
  sendPage(body, "Settings");
}

static void handleSettingsSave() {
  if (server.hasArg("tz")) settings.tzIndex = (uint8_t) server.arg("tz").toInt();
  settings.use24Hour = server.hasArg("use24h");

  if (server.hasArg("zip")) strlcpy(settings.zipCode, server.arg("zip").c_str(), sizeof(settings.zipCode));
  if (server.hasArg("wxint")) settings.weatherIntervalMin = (uint16_t) server.arg("wxint").toInt();
  settings.useCelsius = server.hasArg("usec");

  settings.autoRotate = server.hasArg("autorot");
  if (server.hasArg("rotsec")) settings.rotateIntervalSec = (uint16_t) server.arg("rotsec").toInt();
  if (server.hasArg("bright")) settings.brightness = (uint8_t) server.arg("bright").toInt();

  settings.scheduleEnabled = server.hasArg("schen");
  if (server.hasArg("schoffh")) settings.scheduleOffHour = (uint8_t) server.arg("schoffh").toInt();
  if (server.hasArg("schoffm")) settings.scheduleOffMinute = (uint8_t) server.arg("schoffm").toInt();
  if (server.hasArg("schonh")) settings.scheduleOnHour = (uint8_t) server.arg("schonh").toInt();
  if (server.hasArg("schonm")) settings.scheduleOnMinute = (uint8_t) server.arg("schonm").toInt();

  if (server.hasArg("otapw") && server.arg("otapw").length() > 0) {
    strlcpy(settings.otaPassword, server.arg("otapw").c_str(), sizeof(settings.otaPassword));
  }

  configSave();
  timeApplyTimezone();
  displayApplyBrightness();
  weatherRequestRefresh();

  redirectTo("/settings");
}

// ---------------------------------------------------------------------------
// Wi-Fi setup
// ---------------------------------------------------------------------------
static void handleWifiForm() {
  String body = "<form method='POST' action='/wifi'>";
  body += "<label>SSID<input name='ssid' value='" + String(settings.wifiSSID) + "'></label>";
  body += "<label>Password<input type='password' name='pass' value=''></label>";
  body += "<input type='submit' value='Connect'></form>";
  body += "<p>" + wifiStatusText() + "</p>";
  sendPage(body, "Wi-Fi");
}

static void handleWifiSave() {
  if (server.hasArg("ssid")) strlcpy(settings.wifiSSID, server.arg("ssid").c_str(), sizeof(settings.wifiSSID));
  if (server.hasArg("pass") && server.arg("pass").length() > 0) {
    strlcpy(settings.wifiPass, server.arg("pass").c_str(), sizeof(settings.wifiPass));
  }
  configSave();
  wifiApplyNewCredentials();
  redirectTo("/wifi");
}

// ---------------------------------------------------------------------------
// Factory reset
// ---------------------------------------------------------------------------
static void handleFactoryResetForm() {
  String body = "<p>This will erase Wi-Fi credentials and reset all settings to defaults.</p>";
  body += "<form method='POST' action='/factory-reset'><input type='submit' value='Confirm Factory Reset'></form>";
  sendPage(body, "Factory Reset");
}

static void handleFactoryResetDo() {
  configFactoryReset();
  server.send(200, "text/html", htmlHeader("Factory Reset") + "<p>Done. Rebooting...</p>" + htmlFooter);
  delay(500);
  ESP.restart();
}

// ---------------------------------------------------------------------------
// Firmware update (Basic Auth protected, per project spec)
// ---------------------------------------------------------------------------
static void handleUpdateForm() {
  if (!server.authenticate("admin", settings.otaPassword)) return server.requestAuthentication();
  String body = "<form method='POST' action='/update' enctype='multipart/form-data'>";
  body += "<label>Compiled firmware .bin<input type='file' name='firmware'></label>";
  body += "<input type='submit' value='Upload &amp; Flash'></form>";
  body += "<p>Current version: " + String(FW_VERSION) + " (" + String(FW_BUILD) + ")</p>";
  sendPage(body, "Firmware Update");
}

static void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (!server.authenticate("admin", settings.otaPassword)) {
      updateAuthFailed = true;
      return;
    }
    updateAuthFailed = false;
    Serial.printf("[OTA] starting update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (updateAuthFailed) return;
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (updateAuthFailed) return;
    if (Update.end(true)) {
      Serial.printf("[OTA] update success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

static void handleUpdateFinish() {
  if (updateAuthFailed) {
    server.requestAuthentication();
    return;
  }
  server.sendHeader("Connection", "close");
  if (Update.hasError()) {
    server.send(200, "text/html", htmlHeader("Firmware Update") + "<p>Update FAILED. Check the file and try again.</p>" + htmlFooter);
  } else {
    server.send(200, "text/html", htmlHeader("Firmware Update") + "<p>Update OK. Rebooting... settings are preserved.</p>" + htmlFooter);
    delay(500);
    ESP.restart();
  }
}

// ---------------------------------------------------------------------------
void webInit() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/screen", HTTP_GET, handleScreen);

  server.on("/settings", HTTP_GET, handleSettingsForm);
  server.on("/settings", HTTP_POST, handleSettingsSave);

  server.on("/wifi", HTTP_GET, handleWifiForm);
  server.on("/wifi", HTTP_POST, handleWifiSave);

  server.on("/factory-reset", HTTP_GET, handleFactoryResetForm);
  server.on("/factory-reset", HTTP_POST, handleFactoryResetDo);

  server.on("/update", HTTP_GET, handleUpdateForm);
  server.on("/update", HTTP_POST, handleUpdateFinish, handleUpdateUpload);

  server.begin();
}

void webHandleClient() {
  server.handleClient();
}
