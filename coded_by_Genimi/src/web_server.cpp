#include "web_server.h"
#include "config.h"
#include "weather_mgr.h"
#include "display_mgr.h"
#include <ESPAsyncWebServer.h>
#include <Update.h>

static AsyncWebServer server(80);

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><title>ESP32 OLED Clock</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{font-family:Arial;margin:20px;background:#f4f4f4;color:#333;}
.card{background:#fff;padding:15px;border-radius:8px;margin-bottom:15px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}
h2{margin-top:0;}label{display:block;margin-top:8px;}
input,select{width:100%;padding:8px;margin-top:4px;box-sizing:border-box;}
button{background:#007bff;color:#fff;border:none;padding:10px 15px;border-radius:4px;cursor:pointer;margin-top:12px;}
</style></head><body>
<h1>OLED Clock & Weather Config</h1>
<div class="card">
  <h2>Settings</h2>
  <form action="/save" method="POST">
    <label>ZIP Code: <input type="text" name="zip" value="%ZIP%"></label>
    <label>Weather Refresh (Min): <input type="number" name="wref" value="%WREF%"></label>
    <label>Timezone (POSIX): <input type="text" name="tz" value="%TZ%"></label>
    <label>OLED Brightness (0-255): <input type="number" name="bright" value="%BRIGHT%"></label>
    <button type="submit">Save & Restart</button>
  </form>
</div>
<div class="card">
  <h2>Actions</h2>
  <button onclick="fetch('/toggle-screen')">Toggle Display ON/OFF</button>
  <button onclick="fetch('/reset')" style="background:#dc3545;">Factory Reset</button>
</div>
<div class="card">
  <h2>Firmware OTA Update</h2>
  <form method="POST" action="/update" enctype="multipart/form-data">
    <label>Password: <input type="password" name="pass"></label>
    <label>Select Firmware .bin: <input type="file" name="update"></label>
    <button type="submit">Upload & Flash</button>
  </form>
</div>
</body></html>
)rawliteral";

String processor(const String& var) {
  if (var == "ZIP") return String(sysCfg.zipCode);
  if (var == "WREF") return String(sysCfg.weatherRefreshMin);
  if (var == "TZ") return String(sysCfg.posixTz);
  if (var == "BRIGHT") return String(sysCfg.oledBrightness);
  return String();
}

void initWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML, processor);
  });

  server.on("/toggle-screen", HTTP_GET, [](AsyncWebServerRequest *request) {
    toggleScreenPower();
    request->send(200, "text/plain", "OK");
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("zip", true)) strncpy(sysCfg.zipCode, request->getParam("zip", true)->value().c_str(), sizeof(sysCfg.zipCode));
    if (request->hasParam("wref", true)) sysCfg.weatherRefreshMin = request->getParam("wref", true)->value().toInt();
    if (request->hasParam("tz", true)) strncpy(sysCfg.posixTz, request->getParam("tz", true)->value().c_str(), sizeof(sysCfg.posixTz));
    if (request->hasParam("bright", true)) sysCfg.oledBrightness = request->getParam("bright", true)->value().toInt();
    saveConfiguration();
    request->send(200, "text/html", "Settings saved. Rebooting... <a href='/'>Back</a>");
    delay(1000);
    ESP.restart();
  });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    resetConfigurationToDefaults();
    request->send(200, "text/plain", "Factory Reset Applied. Rebooting...");
    delay(1000);
    ESP.restart();
  });

  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (Update.hasError()) ? "OTA FAIL" : "OTA SUCCESS. Rebooting...");
    response->addHeader("Connection", "close");
    request->send(response);
    delay(1000);
    ESP.restart();
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      // Start OTA update with maximum available flash space
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        Update.printError(Serial);
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }
    }
    if (final) {
      if (Update.end(true)) {
        Serial.println("OTA Update Successful");
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
}