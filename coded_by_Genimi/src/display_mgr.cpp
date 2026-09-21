#include "display_mgr.h"
#include "time_mgr.h"
#include "weather_mgr.h"
#include <WiFi.h>

// SH1106 128x64 I2C OLED Driver
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA);

static DisplayMode currentMode = MODE_LARGE_TIME;
static bool displayPower = true;
static unsigned long lastRotationMs = 0;
static unsigned long bootTimeMs = 0;

void initDisplayManager() {
  u8g2.begin();
  u8g2.setContrast(sysCfg.oledBrightness);
  bootTimeMs = millis();
}

static void drawWifiIcon(int x, int y) {
  if (WiFi.status() == WL_CONNECTED) {
    u8g2.drawDisc(x + 4, y + 6, 1);
    u8g2.drawCircle(x + 4, y + 6, 3, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
    u8g2.drawCircle(x + 4, y + 6, 5, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
  } else {
    u8g2.drawLine(x, y, x + 8, y + 8);
    u8g2.drawLine(x + 8, y, x, y + 8);
  }
}

static void renderCurrentMode() {
  u8g2.clearBuffer();
  drawWifiIcon(118, 0);

  switch (currentMode) {
    case MODE_LARGE_TIME: {
      u8g2.setFont(u8g2_font_logisoso28_tr);
      String t = getFormattedTime(false);
      u8g2.drawStr((128 - u8g2.getStrWidth(t.c_str())) / 2, 48, t.c_str());
      break;
    }
    case MODE_SMALL_TIME_SEC: {
      u8g2.setFont(u8g2_font_ncenB14_tr);
      String t = getFormattedTime(true);
      u8g2.drawStr((128 - u8g2.getStrWidth(t.c_str())) / 2, 40, t.c_str());
      break;
    }
    case MODE_TIME_DATE: {
      u8g2.setFont(u8g2_font_ncenB12_tr);
      String t = getFormattedTime(false);
      u8g2.drawStr((128 - u8g2.getStrWidth(t.c_str())) / 2, 28, t.c_str());
      u8g2.setFont(u8g2_font_6x10_tr);
      String d = getFormattedDate();
      u8g2.drawStr((128 - u8g2.getStrWidth(d.c_str())) / 2, 50, d.c_str());
      break;
    }
    case MODE_TIME_DATE_WEATHER: {
      u8g2.setFont(u8g2_font_6x10_tr);
      String t = getFormattedTime(false) + " | " + getFormattedDate();
      u8g2.drawStr(0, 15, t.c_str());

      String w = currentWeather.isValid 
                 ? String(currentWeather.temp, 1) + (sysCfg.tempUnitF ? " F" : " C") + (currentWeather.isStale ? " (*)" : "")
                 : "Weather: --";
      u8g2.drawStr(0, 45, w.c_str());
      break;
    }
    case MODE_TIME_WEATHER: {
      u8g2.setFont(u8g2_font_ncenB14_tr);
      String t = getFormattedTime(false);
      u8g2.drawStr(0, 24, t.c_str());

      u8g2.setFont(u8g2_font_9x15_tr);
      String w = currentWeather.isValid 
                 ? String((int)currentWeather.temp) + (sysCfg.tempUnitF ? "\xb0 F" : "\xb0 C")
                 : "NO DATA";
      u8g2.drawStr(0, 56, w.c_str());
      break;
    }
    case MODE_SYS_INFO: {
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.drawStr(0, 10, ("FW: v" + String(FW_VERSION)).c_str());
      u8g2.drawStr(0, 24, ("Build: " + String(__DATE__)).c_str());

      unsigned long sec = (millis() - bootTimeMs) / 1000;
      String uptime = "Uptime: " + String(sec / 3600) + "h " + String((sec % 3600) / 60) + "m";
      u8g2.drawStr(0, 38, uptime.c_str());
      break;
    }
    default: break;
  }
  u8g2.sendBuffer();
}

void loopDisplayManager() {
  // Screen Scheduling
  if (sysCfg.schedScreenEnable) {
    uint8_t h = getCurrentHour();
    if (sysCfg.screenOnHour < sysCfg.screenOffHour) {
      setScreenPower(h >= sysCfg.screenOnHour && h < sysCfg.screenOffHour);
    } else {
      setScreenPower(h >= sysCfg.screenOnHour || h < sysCfg.screenOffHour);
    }
  }

  if (!displayPower) return;

  // Auto Rotation Check
  if (sysCfg.autoRotate && sysCfg.rotateIntervalSec > 0) {
    if (millis() - lastRotationMs > (sysCfg.rotateIntervalSec * 1000UL)) {
      nextDisplayMode();
    }
  }

  static unsigned long lastRender = 0;
  if (millis() - lastRender > 250) { // Render 4FPS non-blocking
    lastRender = millis();
    renderCurrentMode();
  }
}

void nextDisplayMode() {
  currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
  lastRotationMs = millis();
}

void setScreenPower(bool on) {
  displayPower = on;
  u8g2.setPowerSave(on ? 0 : 1);
}

void toggleScreenPower() {
  setScreenPower(!displayPower);
}

bool isScreenOn() {
  return displayPower;
}