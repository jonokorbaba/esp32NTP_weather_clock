#pragma once
#include <Arduino.h>
#include <time.h>

struct TimeZoneEntry {
  const char* label;
  const char* posixTZ;
};

// US mainland timezones. POSIX TZ strings encode both the UTC offset and the
// US DST rule, so no manual DST bookkeeping is needed.
static const TimeZoneEntry TIMEZONES[] = {
  {"Eastern",             "EST5EDT,M3.2.0,M11.1.0"},
  {"Central",             "CST6CDT,M3.2.0,M11.1.0"},
  {"Mountain",            "MST7MDT,M3.2.0,M11.1.0"},
  {"Arizona (MST, no DST)", "MST7"},
  {"Pacific",             "PST8PDT,M3.2.0,M11.1.0"},
};
#define TIMEZONE_COUNT (sizeof(TIMEZONES) / sizeof(TIMEZONES[0]))

void timeInit();                 // call once at boot
void timeApplyTimezone();        // re-applies settings.tzIndex (call after it changes)
void timeOnWifiConnected();      // kick off an NTP sync attempt
void timeMaybeResync();          // call every loop; resyncs hourly while connected

bool timeIsSynced();
time_t timeLastSyncEpoch();

// Non-blocking "give me local time if you have it" — returns false (without
// blocking) if time has never been synced yet.
bool timeGetLocal(struct tm* out);
