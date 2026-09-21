#include "config.h"
#include <Preferences.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

Settings settings;

static Preferences prefs;
static const char* NS = "clockcfg";

// NVS/Preferences isn't safe to hit from two tasks at once, and both the
// main loop (Web UI saves) and the weather task (core 0, on a ZIP change)
// call configSave(), so all access is serialized through this mutex.
static SemaphoreHandle_t g_cfgMutex = nullptr;
static void ensureMutex() {
  if (!g_cfgMutex) g_cfgMutex = xSemaphoreCreateMutex();
}

void configLoad() {
  ensureMutex();
  xSemaphoreTake(g_cfgMutex, portMAX_DELAY);
  prefs.begin(NS, false);

  String ssid = prefs.getString("ssid", settings.wifiSSID);
  String pass = prefs.getString("pass", settings.wifiPass);
  strlcpy(settings.wifiSSID, ssid.c_str(), sizeof(settings.wifiSSID));
  strlcpy(settings.wifiPass, pass.c_str(), sizeof(settings.wifiPass));

  settings.use24Hour = prefs.getBool("use24h", settings.use24Hour);
  settings.tzIndex    = prefs.getUChar("tz", settings.tzIndex);

  String zip = prefs.getString("zip", settings.zipCode);
  strlcpy(settings.zipCode, zip.c_str(), sizeof(settings.zipCode));
  settings.weatherIntervalMin = prefs.getUShort("wxint", settings.weatherIntervalMin);
  settings.useCelsius         = prefs.getBool("usec", settings.useCelsius);
  settings.cachedLat          = prefs.getFloat("lat", settings.cachedLat);
  settings.cachedLon          = prefs.getFloat("lon", settings.cachedLon);
  settings.hasCoords          = prefs.getBool("hascoord", settings.hasCoords);
  String coordsZip = prefs.getString("coordzip", settings.coordsForZip);
  strlcpy(settings.coordsForZip, coordsZip.c_str(), sizeof(settings.coordsForZip));

  settings.autoRotate       = prefs.getBool("autorot", settings.autoRotate);
  settings.rotateIntervalSec = prefs.getUShort("rotsec", settings.rotateIntervalSec);
  settings.brightness        = prefs.getUChar("bright", settings.brightness);

  settings.scheduleEnabled  = prefs.getBool("schen", settings.scheduleEnabled);
  settings.scheduleOffHour   = prefs.getUChar("schoffh", settings.scheduleOffHour);
  settings.scheduleOffMinute = prefs.getUChar("schoffm", settings.scheduleOffMinute);
  settings.scheduleOnHour    = prefs.getUChar("schonh", settings.scheduleOnHour);
  settings.scheduleOnMinute  = prefs.getUChar("schonm", settings.scheduleOnMinute);

  String otaPw = prefs.getString("otapw", settings.otaPassword);
  strlcpy(settings.otaPassword, otaPw.c_str(), sizeof(settings.otaPassword));

  prefs.end();
  xSemaphoreGive(g_cfgMutex);
}

void configSave() {
  ensureMutex();
  xSemaphoreTake(g_cfgMutex, portMAX_DELAY);
  prefs.begin(NS, false);

  prefs.putString("ssid", settings.wifiSSID);
  prefs.putString("pass", settings.wifiPass);

  prefs.putBool("use24h", settings.use24Hour);
  prefs.putUChar("tz", settings.tzIndex);

  prefs.putString("zip", settings.zipCode);
  prefs.putUShort("wxint", settings.weatherIntervalMin);
  prefs.putBool("usec", settings.useCelsius);
  prefs.putFloat("lat", settings.cachedLat);
  prefs.putFloat("lon", settings.cachedLon);
  prefs.putBool("hascoord", settings.hasCoords);
  prefs.putString("coordzip", settings.coordsForZip);

  prefs.putBool("autorot", settings.autoRotate);
  prefs.putUShort("rotsec", settings.rotateIntervalSec);
  prefs.putUChar("bright", settings.brightness);

  prefs.putBool("schen", settings.scheduleEnabled);
  prefs.putUChar("schoffh", settings.scheduleOffHour);
  prefs.putUChar("schoffm", settings.scheduleOffMinute);
  prefs.putUChar("schonh", settings.scheduleOnHour);
  prefs.putUChar("schonm", settings.scheduleOnMinute);

  prefs.putString("otapw", settings.otaPassword);

  prefs.end();
  xSemaphoreGive(g_cfgMutex);
}

void configFactoryReset() {
  ensureMutex();
  xSemaphoreTake(g_cfgMutex, portMAX_DELAY);
  prefs.begin(NS, false);
  prefs.clear();
  prefs.end();
  xSemaphoreGive(g_cfgMutex);

  Settings fresh; // default-constructed = defaults from config.h
  settings = fresh;
  configSave();
}
