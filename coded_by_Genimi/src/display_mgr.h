#ifndef DISPLAY_MGR_H
#define DISPLAY_MGR_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

enum DisplayMode {
  MODE_LARGE_TIME = 0,
  MODE_SMALL_TIME_SEC,
  MODE_TIME_DATE,
  MODE_TIME_DATE_WEATHER,
  MODE_TIME_WEATHER,
  MODE_SYS_INFO,
  MODE_COUNT
};

void initDisplayManager();
void loopDisplayManager();
void nextDisplayMode();
void toggleScreenPower();
void setScreenPower(bool on);
bool isScreenOn();

#endif