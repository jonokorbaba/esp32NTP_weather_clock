#ifndef WEATHER_MGR_H
#define WEATHER_MGR_H

#include <Arduino.h>
#include "config.h"

struct WeatherData {
  float temp;
  int weatherCode;
  bool isValid;
  bool isStale;
  unsigned long lastSuccessTime;
};

extern WeatherData currentWeather;

void initWeatherManager();
void loopWeatherManager();
void triggerWeatherFetch();
uint16_t getNextWeatherSyncCountdownMin();

#endif