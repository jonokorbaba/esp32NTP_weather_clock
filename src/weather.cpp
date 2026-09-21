#include "weather.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ---------------------------------------------------------------------------
// All network I/O for weather runs on a dedicated FreeRTOS task pinned to
// core 0, so a slow or failed HTTP request can never stall the display loop
// (which runs on core 1 along with everything else in main.cpp). The task
// publishes results into `g_snapshot`, guarded by a mutex; the rest of the
// firmware only ever reads a copy via weatherGetSnapshot().
// ---------------------------------------------------------------------------

static WeatherSnapshot g_snapshot;
static SemaphoreHandle_t g_mutex = nullptr;
static volatile bool g_forceRefresh = false;

const char* weatherConditionLabel(WeatherCondition c) {
  switch (c) {
    case WX_CLEAR:         return "Clear";
    case WX_PARTLY_CLOUDY: return "Partly Cloudy";
    case WX_CLOUDY:        return "Cloudy";
    case WX_FOG:           return "Fog";
    case WX_DRIZZLE:       return "Drizzle";
    case WX_RAIN:          return "Rain";
    case WX_SNOW:          return "Snow";
    case WX_THUNDERSTORM:  return "Thunderstorm";
    default:               return "Unknown";
  }
}

static WeatherCondition mapWmoCode(int code) {
  if (code == 0) return WX_CLEAR;
  if (code == 1 || code == 2) return WX_PARTLY_CLOUDY;
  if (code == 3) return WX_CLOUDY;
  if (code == 45 || code == 48) return WX_FOG;
  if (code >= 51 && code <= 57) return WX_DRIZZLE;
  if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return WX_RAIN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return WX_SNOW;
  if (code >= 95 && code <= 99) return WX_THUNDERSTORM;
  return WX_UNKNOWN;
}

WeatherSnapshot weatherGetSnapshot() {
  WeatherSnapshot copy;
  if (g_mutex && xSemaphoreTake(g_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    copy = g_snapshot;
    xSemaphoreGive(g_mutex);
  }
  return copy;
}

void weatherRequestRefresh() {
  g_forceRefresh = true;
}

// Open-Meteo's forecast API takes latitude/longitude, not ZIP codes, so a US
// ZIP has to be resolved to coordinates first. Zippopotam.us is a free,
// no-API-key lookup that's a good fit for that one job.
static bool resolveZipToCoords(const char* zip, float &lat, float &lon) {
  WiFiClientSecure client;
  client.setInsecure(); // no cert pinning — simple GET, not sensitive data
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  String url = "https://api.zippopotam.us/us/" + String(zip);
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    String payload = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
      JsonArray places = doc["places"];
      if (places.size() > 0) {
        lat = places[0]["latitude"].as<float>();
        lon = places[0]["longitude"].as<float>();
        ok = true;
      }
    }
  }
  http.end();
  return ok;
}

static bool fetchForecast(float lat, float lon, bool celsius, float &outTemp, int &outCode) {
  WiFiClientSecure client;
  client.setInsecure(); // no cert pinning — simple GET, not sensitive data
  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(8000);

  char url[256];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,weather_code&temperature_unit=%s&timezone=auto",
    lat, lon, celsius ? "celsius" : "fahrenheit");

  if (!http.begin(client, url)) return false;
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    String payload = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
      JsonObject cur = doc["current"];
      if (!cur.isNull()) {
        outTemp = cur["temperature_2m"].as<float>();
        outCode = cur["weather_code"].as<int>();
        ok = true;
      }
    }
  }
  http.end();
  return ok;
}

static void weatherTask(void* /*pv*/) {
  for (;;) {
    bool wifiUp = (WiFi.status() == WL_CONNECTED);
    time_t now = time(nullptr);

    bool intervalDue;
    {
      WeatherSnapshot snap = weatherGetSnapshot();
      intervalDue = (snap.nextAttemptEpoch == 0) || (now >= snap.nextAttemptEpoch);
    }

    if (wifiUp && (g_forceRefresh || intervalDue)) {
      g_forceRefresh = false;

      // NOTE: settings.cachedLat/cachedLon/hasCoords/coordsForZip are shared
      // with the main loop (Web UI can read them). Writes here are
      // infrequent (only when the ZIP actually changes) and the Web UI never
      // writes these particular fields, so this is left unguarded for
      // simplicity rather than adding a second mutex.
      bool needCoords = !settings.hasCoords ||
                         strcmp(settings.coordsForZip, settings.zipCode) != 0;
      bool coordsOk = settings.hasCoords;
      float lat = settings.cachedLat, lon = settings.cachedLon;

      if (needCoords) {
        coordsOk = resolveZipToCoords(settings.zipCode, lat, lon);
        if (coordsOk) {
          settings.cachedLat = lat;
          settings.cachedLon = lon;
          settings.hasCoords = true;
          strlcpy(settings.coordsForZip, settings.zipCode, sizeof(settings.coordsForZip));
          configSave();
        }
      }

      float temp = 0; int wmo = 0;
      bool success = coordsOk && fetchForecast(lat, lon, settings.useCelsius, temp, wmo);

      uint32_t intervalSec = (uint32_t)settings.weatherIntervalMin * 60UL;
      if (intervalSec < 60) intervalSec = 60; // guard against a bad config value

      if (xSemaphoreTake(g_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        if (success) {
          g_snapshot.everSucceeded = true;
          g_snapshot.stale = false;
          g_snapshot.temperature = temp;
          g_snapshot.condition = mapWmoCode(wmo);
          g_snapshot.lastSuccessEpoch = time(nullptr);
          g_snapshot.lastError[0] = '\0';
        } else {
          // Keep whatever values are already cached — only the flags/error
          // change, so the display keeps showing the last good reading.
          g_snapshot.stale = g_snapshot.everSucceeded;
          strlcpy(g_snapshot.lastError,
                  coordsOk ? "forecast request failed" : "ZIP lookup failed",
                  sizeof(g_snapshot.lastError));
        }
        g_snapshot.nextAttemptEpoch = time(nullptr) + intervalSec;
        xSemaphoreGive(g_mutex);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void weatherInit() {
  g_mutex = xSemaphoreCreateMutex();
  // Generous stack: TLS (WiFiClientSecure/mbedTLS) + HTTPClient + JSON
  // parsing all run in this task and are stack-hungry.
  xTaskCreatePinnedToCore(weatherTask, "weatherTask", 16384, nullptr, 1, nullptr, 0);
}
