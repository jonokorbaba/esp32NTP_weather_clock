#pragma once
#include <Arduino.h>
#include "config.h"

void displayInit();

// Call every loop() iteration. Internally throttles actual redraws so it's
// cheap to call constantly; also owns the schedule/auto-rotate/wake logic.
void displayUpdate();

// Call every loop() iteration to poll the TTP223 touch pin.
void displayHandleTouch();

// Web UI hooks:
void displaySetManualState(ManualScreenState s);
void displayApplyBrightness(); // call after settings.brightness changes
bool displayIsAwake();
DisplayMode displayCurrentMode();
const char* displayModeLabel(DisplayMode m);
