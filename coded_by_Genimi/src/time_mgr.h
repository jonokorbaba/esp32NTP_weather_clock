#ifndef TIME_MGR_H
#define TIME_MGR_H

#include <Arduino.h>
#include "config.h"

void initTimeManager();
void loopTimeManager();
bool isNtpSynced();
String getFormattedTime(bool includeSeconds);
String getFormattedDate();
uint8_t getCurrentHour();

#endif