#include "time_mgr.h"
#include <WiFi.h>
#include <time.h>

static unsigned long lastNtpAttempt = 0;
static const unsigned long NTP_INTERVAL_MS = 3600000; // 1 hour
static bool ntpSyncedOnce = false;

void initTimeManager() {
  configTzTime(sysCfg.posixTz, "pool.ntp.org", "time.nist.gov");
}

void loopTimeManager() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!ntpSyncedOnce || (millis() - lastNtpAttempt > NTP_INTERVAL_MS)) {
      lastNtpAttempt = millis();
      configTzTime(sysCfg.posixTz, "pool.ntp.org", "time.nist.gov");
    }
  }
}

bool isNtpSynced() {
  time_t now;
  time(&now);
  struct tm timeinfo;
  if (localtime_r(&now, &timeinfo)) {
    if (timeinfo.tm_year > (2020 - 1900)) {
      ntpSyncedOnce = true;
      return true;
    }
  }
  return false;
}

String getFormattedTime(bool includeSeconds) {
  time_t now;
  time(&now);
  struct tm t;
  if (!localtime_r(&now, &t)) return "00:00";

  char buf[16];
  if (sysCfg.is12Hour) {
    int hour12 = t.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    if (includeSeconds) {
      snprintf(buf, sizeof(buf), "%d:%02d:%02d %s", hour12, t.tm_min, t.tm_sec, (t.tm_hour >= 12) ? "PM" : "AM");
    } else {
      snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, t.tm_min, (t.tm_hour >= 12) ? "PM" : "AM");
    }
  } else {
    if (includeSeconds) {
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    } else {
      snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    }
  }
  return String(buf);
}

String getFormattedDate() {
  time_t now;
  time(&now);
  struct tm t;
  if (!localtime_r(&now, &t)) return "Jan 01, 2026";
  char buf[20];
  strftime(buf, sizeof(buf), "%b %d, %Y", &t);
  return String(buf);
}

uint8_t getCurrentHour() {
  time_t now;
  time(&now);
  struct tm t;
  if (!localtime_r(&now, &t)) return 0;
  return t.tm_hour;
}