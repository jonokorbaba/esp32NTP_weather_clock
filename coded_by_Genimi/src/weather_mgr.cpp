#include "weather_mgr.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

WeatherData currentWeather = {0.0f, 0, false, true, 0};

static unsigned long lastFetchAttempt = 0;
static float cachedLat = 0.0f;
static float cachedLon = 0.0f;
static bool coordsValid = false;

static bool geocodeZip(const char* zip, float &lat, float &lon) {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  HTTPClient http;
  http.setTimeout(4000);
  String url = "http://api.zippopotam.us/us/" + String(zip);
  if (!http.begin(url)) return false;

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (!err) {
      lat = doc["places"][0]["latitude"].as<float>();
      lon = doc["places"][0]["longitude"].as<float>();
      return true;
    }
  }
  http.end();
  return false;
}

static bool fetchOpenMeteo(float lat, float lon) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.setTimeout(4000);
  String unitStr = sysCfg.tempUnitF ? "&temperature_unit=fahrenheit" : "";
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
               "&longitude=" + String(lon, 4) + "&current_weather=true" + unitStr;

  if (!http.begin(url)) return false;

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();
    if (!err) {
      currentWeather.temp = doc["current_weather"]["temperature"].as<float>();
      currentWeather.weatherCode = doc["current_weather"]["weathercode"].as<int>();
      currentWeather.isValid = true;
      currentWeather.isStale = false;
      currentWeather.lastSuccessTime = millis();
      return true;
    }
  }
  http.end();
  return false;
}

void initWeatherManager() {
  triggerWeatherFetch();
}

void triggerWeatherFetch() {
  lastFetchAttempt = millis();
  if (!coordsValid) {
    coordsValid = geocodeZip(sysCfg.zipCode, cachedLat, cachedLon);
  }
  if (coordsValid) {
    if (!fetchOpenMeteo(cachedLat, cachedLon)) {
      currentWeather.isStale = true;
    }
  } else {
    currentWeather.isStale = true;
  }
}

void loopWeatherManager() {
  unsigned long intervalMs = (unsigned long)sysCfg.weatherRefreshMin * 60000UL;
  if (millis() - lastFetchAttempt > intervalMs || lastFetchAttempt == 0) {
    if (WiFi.status() == WL_CONNECTED) {
      triggerWeatherFetch();
    }
  }
}

uint16_t getNextWeatherSyncCountdownMin() {
  unsigned long intervalMs = (unsigned long)sysCfg.weatherRefreshMin * 60000UL;
  unsigned long elapsed = millis() - lastFetchAttempt;
  if (elapsed >= intervalMs) return 0;
  return (uint16_t)((intervalMs - elapsed) / 60000UL);
}