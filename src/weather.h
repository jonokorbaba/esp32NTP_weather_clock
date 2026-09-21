#pragma once
#include <Arduino.h>

enum WeatherCondition : uint8_t {
  WX_UNKNOWN = 0,
  WX_CLEAR,
  WX_PARTLY_CLOUDY,
  WX_CLOUDY,
  WX_FOG,
  WX_DRIZZLE,
  WX_RAIN,
  WX_SNOW,
  WX_THUNDERSTORM
};

struct WeatherSnapshot {
  bool everSucceeded = false;   // has a fetch ever completed successfully
  bool stale = false;           // last attempt failed; values below are cached
  float temperature = 0.0f;     // in the unit selected in settings (F or C)
  WeatherCondition condition = WX_UNKNOWN;
  time_t lastSuccessEpoch = 0;  // 0 = never
  time_t nextAttemptEpoch = 0;  // 0 = unknown / not scheduled yet
  char lastError[48] = "";
};

// Starts the background weather task (pinned to core 0). Call once from setup().
void weatherInit();

// Thread-safe copy of the latest snapshot, for rendering / the Web UI.
WeatherSnapshot weatherGetSnapshot();

// Ask the background task to refresh as soon as possible (e.g. right after
// the ZIP code or units are changed in the Web UI). Non-blocking.
void weatherRequestRefresh();

const char* weatherConditionLabel(WeatherCondition c);
