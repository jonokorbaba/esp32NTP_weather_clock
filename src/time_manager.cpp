#include "time_manager.h"
#include "config.h"
#include <WiFi.h>

// configTime()/SNTP on ESP32 Arduino is inherently async: the call returns
// immediately and the underlying SNTP client updates the system clock in the
// background whenever a response arrives. We never block waiting for it —
// we just periodically ask "has the clock moved past a sane date yet?".

static unsigned long lastSyncTriggerMillis = 0;
static const unsigned long RESYNC_INTERVAL_MS = 3600000UL; // 1 hour, per spec
static const time_t SANE_EPOCH_THRESHOLD = 1700000000;     // ~Nov 2023
static time_t lastKnownGoodEpoch = 0;

void timeApplyTimezone() {
  uint8_t idx = settings.tzIndex;
  if (idx >= TIMEZONE_COUNT) idx = 0;
  setenv("TZ", TIMEZONES[idx].posixTZ, 1);
  tzset();
}

void timeInit() {
  timeApplyTimezone();
}

void timeOnWifiConnected() {
  // gmtOffset/daylightOffset are left at 0 — the TZ env var (set above)
  // already encodes both the UTC offset and the US DST rule, and localtime_r
  // reads TZ directly, so there's nothing else to configure here.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  lastSyncTriggerMillis = millis();
}

void timeMaybeResync() {
  if (WiFi.status() == WL_CONNECTED &&
      (millis() - lastSyncTriggerMillis > RESYNC_INTERVAL_MS)) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    lastSyncTriggerMillis = millis();
  }
}

bool timeIsSynced() {
  time_t t = time(nullptr);
  if (t > SANE_EPOCH_THRESHOLD) {
    lastKnownGoodEpoch = t;
    return true;
  }
  return false;
}

time_t timeLastSyncEpoch() {
  return lastKnownGoodEpoch;
}

bool timeGetLocal(struct tm* out) {
  if (!timeIsSynced()) return false;
  time_t now = time(nullptr);
  localtime_r(&now, out);
  return true;
}
