#include "display.h"
#include "pins.h"
#include "time_manager.h"
#include "weather.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

extern const char* FW_VERSION;
extern const char* FW_BUILD;
extern unsigned long bootMillis;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static ManualScreenState manualState = SCREEN_AUTO;
static bool peekActive = false;
static unsigned long peekExpiresAt = 0;

static DisplayMode currentMode = MODE_BIG_TIME;
static unsigned long lastModeChangeMillis = 0;
static unsigned long lastRenderMillis = 0;
static bool oledPoweredOn = true;

static bool lastTouchLevel = false;
static unsigned long lastTouchEdgeMillis = 0;
static const unsigned long TOUCH_DEBOUNCE_MS = 60;
static const unsigned long PEEK_DURATION_MS = 20000;
static const unsigned long RENDER_INTERVAL_MS = 200; // ~5 fps is plenty for a clock

// ---------------------------------------------------------------------------
// Raw SSD1306 commands (kept independent of the Adafruit library's internals
// so contrast/sleep control works regardless of library version).
// ---------------------------------------------------------------------------
static void sendOledCommand(uint8_t cmd) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00); // control byte: Co=0, D/C=0 -> command stream follows
  Wire.write(cmd);
  Wire.endTransmission();
}

static void setOledContrast(uint8_t value) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(0x81); // set contrast control
  Wire.write(value);
  Wire.endTransmission();
}

void displayApplyBrightness() {
  setOledContrast(settings.brightness);
}

// ---------------------------------------------------------------------------
// Schedule helpers
// ---------------------------------------------------------------------------
static bool isWithinOffWindow(const struct tm &t) {
  int nowMin = t.tm_hour * 60 + t.tm_min;
  int offMin = settings.scheduleOffHour * 60 + settings.scheduleOffMinute;
  int onMin  = settings.scheduleOnHour * 60 + settings.scheduleOnMinute;
  if (offMin == onMin) return false; // degenerate config -> never off
  if (offMin < onMin) {
    return nowMin >= offMin && nowMin < onMin;
  } else {
    return nowMin >= offMin || nowMin < onMin; // wraps past midnight
  }
}

static bool computeAwake(bool synced, const struct tm &t) {
  if (manualState == SCREEN_FORCED_ON) return true;
  if (manualState == SCREEN_FORCED_OFF) return false;

  if (!settings.scheduleEnabled || !synced) return true;

  if (peekActive) {
    if (millis() < peekExpiresAt) return true;
    peekActive = false;
  }
  return !isWithinOffWindow(t);
}

void displaySetManualState(ManualScreenState s) {
  manualState = s;
  peekActive = false;
}

bool displayIsAwake() { return oledPoweredOn; }
DisplayMode displayCurrentMode() { return currentMode; }

const char* displayModeLabel(DisplayMode m) {
  switch (m) {
    case MODE_BIG_TIME:          return "Big Time";
    case MODE_SMALL_TIME_SEC:    return "Time + Seconds";
    case MODE_TIME_DATE:         return "Time + Date";
    case MODE_TIME_DATE_WEATHER: return "Time + Date + Weather";
    case MODE_TIME_WEATHER:      return "Time + Weather";
    case MODE_SYSTEM_INFO:       return "System Info";
    default:                     return "?";
  }
}

// ---------------------------------------------------------------------------
// Touch input (TTP223, active-HIGH digital output)
// ---------------------------------------------------------------------------
static void onTouchPress() {
  struct tm t;
  bool synced = timeGetLocal(&t);
  bool wasAwake = computeAwake(synced, t);

  if (!wasAwake) {
    // First touch while asleep: just wake up, don't change mode.
    manualState = SCREEN_AUTO;
    if (settings.scheduleEnabled && synced && isWithinOffWindow(t)) {
      peekActive = true;
      peekExpiresAt = millis() + PEEK_DURATION_MS;
    }
  } else {
    currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
    lastModeChangeMillis = millis();
  }
}

void displayHandleTouch() {
  bool raw = digitalRead(PIN_TOUCH) == HIGH;
  unsigned long now = millis();
  if (raw != lastTouchLevel && (now - lastTouchEdgeMillis) > TOUCH_DEBOUNCE_MS) {
    lastTouchEdgeMillis = now;
    lastTouchLevel = raw;
    if (raw) onTouchPress(); // act on rising edge only
  }
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------
static void drawWifiIndicator(bool connected) {
  // Three signal bars in the top-right corner; filled = connected.
  int x = OLED_WIDTH - 12, y = 1;
  for (int i = 0; i < 3; i++) {
    int barH = 2 + i * 2;
    int bx = x + i * 4;
    int by = y + (6 - barH);
    if (connected) oled.fillRect(bx, by, 3, barH, SSD1306_WHITE);
    else oled.drawRect(bx, by, 3, barH, SSD1306_WHITE);
  }
}

static void drawWeatherIcon(int x, int y, WeatherCondition c) {
  // Compact ~16x16 icons built from basic shapes — no bitmap assets needed.
  switch (c) {
    case WX_CLEAR:
      oled.fillCircle(x + 8, y + 8, 5, SSD1306_WHITE);
      break;
    case WX_PARTLY_CLOUDY:
      oled.fillCircle(x + 5, y + 6, 4, SSD1306_WHITE);
      oled.fillRoundRect(x + 4, y + 9, 11, 6, 3, SSD1306_WHITE);
      break;
    case WX_CLOUDY:
    case WX_FOG:
      oled.fillRoundRect(x + 1, y + 6, 14, 7, 3, SSD1306_WHITE);
      if (c == WX_FOG) {
        oled.drawFastHLine(x, y + 14, 16, SSD1306_WHITE);
      }
      break;
    case WX_DRIZZLE:
    case WX_RAIN:
    case WX_SNOW: {
      oled.fillRoundRect(x + 1, y + 2, 14, 7, 3, SSD1306_WHITE);
      int drops = (c == WX_DRIZZLE) ? 2 : 3;
      for (int i = 0; i < drops; i++) {
        int lx = x + 3 + i * 4;
        if (c == WX_SNOW) oled.drawPixel(lx, y + 13, SSD1306_WHITE);
        else oled.drawFastVLine(lx, y + 10, 4, SSD1306_WHITE);
      }
      break;
    }
    case WX_THUNDERSTORM:
      oled.fillRoundRect(x + 1, y + 1, 14, 7, 3, SSD1306_WHITE);
      oled.drawLine(x + 7, y + 8, x + 4, y + 12, SSD1306_WHITE);
      oled.drawLine(x + 4, y + 12, x + 8, y + 12, SSD1306_WHITE);
      oled.drawLine(x + 8, y + 12, x + 5, y + 16, SSD1306_WHITE);
      break;
    default:
      oled.drawCircle(x + 8, y + 8, 6, SSD1306_WHITE);
      oled.setTextSize(1);
      oled.setCursor(x + 5, y + 4);
      oled.print("?");
      break;
  }
}

static String formatTime(const struct tm &t, bool withSeconds) {
  char buf[16];
  int hour = t.tm_hour;
  const char* ampm = "";
  if (!settings.use24Hour) {
    ampm = (hour >= 12) ? "PM" : "AM";
    hour = hour % 12;
    if (hour == 0) hour = 12;
  }
  if (withSeconds) snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, t.tm_min, t.tm_sec);
  else snprintf(buf, sizeof(buf), "%02d:%02d", hour, t.tm_min);
  String s = buf;
  if (!settings.use24Hour) { s += " "; s += ampm; }
  return s;
}

static String formatDate(const struct tm &t) {
  static const char* wd[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  static const char* mo[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  char buf[24];
  snprintf(buf, sizeof(buf), "%s, %s %d", wd[t.tm_wday], mo[t.tm_mon], t.tm_mday);
  return String(buf);
}

static String formatTemp(const WeatherSnapshot &w) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%.0f%s", w.temperature, settings.useCelsius ? "C" : "F");
  return String(buf);
}

static String formatUptime() {
  unsigned long s = (millis() - bootMillis) / 1000;
  unsigned long d = s / 86400; s %= 86400;
  unsigned long h = s / 3600; s %= 3600;
  unsigned long m = s / 60;
  char buf[24];
  snprintf(buf, sizeof(buf), "%lud %luh %lum", d, h, m);
  return String(buf);
}

// ---------------------------------------------------------------------------
// Per-mode rendering
// ---------------------------------------------------------------------------
static void renderNoTime() {
  oled.setTextSize(1);
  oled.setCursor(10, 28);
  oled.print(WiFi.status() == WL_CONNECTED ? "Waiting for NTP sync..." : "Waiting for Wi-Fi...");
}

static void renderBigTime(const struct tm &t) {
  bool colonOn = (t.tm_sec % 2) == 0;
  String s = formatTime(t, false);
  if (!colonOn) s.replace(":", " ");
  oled.setTextSize(3);
  int16_t x1, y1; uint16_t w, h;
  oled.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((OLED_WIDTH - w) / 2, (OLED_HEIGHT - h) / 2);
  oled.print(s);
}

static void renderSmallTimeSeconds(const struct tm &t) {
  String s = formatTime(t, true);
  oled.setTextSize(2);
  int16_t x1, y1; uint16_t w, h;
  oled.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((OLED_WIDTH - w) / 2, (OLED_HEIGHT - h) / 2);
  oled.print(s);
}

static void renderTimeDate(const struct tm &t) {
  oled.setTextSize(2);
  oled.setCursor(4, 14);
  oled.print(formatTime(t, false));
  oled.setTextSize(1);
  oled.setCursor(4, 42);
  oled.print(formatDate(t));
}

static void renderTimeDateWeather(const struct tm &t, const WeatherSnapshot &w) {
  oled.setTextSize(2);
  oled.setCursor(2, 2);
  oled.print(formatTime(t, false));
  oled.setTextSize(1);
  oled.setCursor(2, 22);
  oled.print(formatDate(t));

  oled.drawFastHLine(0, 34, OLED_WIDTH, SSD1306_WHITE);

  if (w.everSucceeded) {
    drawWeatherIcon(2, 38, w.condition);
    oled.setCursor(22, 40);
    oled.print(formatTemp(w));
    oled.print(w.stale ? " (old)" : "");
    oled.setCursor(22, 50);
    oled.print(weatherConditionLabel(w.condition));
  } else {
    oled.setCursor(2, 44);
    oled.print("Weather: --");
  }
}

static void renderTimeWeather(const struct tm &t, const WeatherSnapshot &w) {
  oled.setTextSize(2);
  int16_t x1, y1; uint16_t tw, th;
  String ts = formatTime(t, false);
  oled.getTextBounds(ts, 0, 0, &x1, &y1, &tw, &th);
  oled.setCursor((OLED_WIDTH - tw) / 2, 2);
  oled.print(ts);

  oled.drawFastHLine(0, 24, OLED_WIDTH, SSD1306_WHITE);

  if (w.everSucceeded) {
    drawWeatherIcon(6, 32, w.condition);
    oled.setTextSize(2);
    oled.setCursor(28, 34);
    oled.print(formatTemp(w));
    oled.setTextSize(1);
    oled.setCursor(28, 52);
    oled.print(weatherConditionLabel(w.condition));
    if (w.stale) { oled.setCursor(90, 52); oled.print("old"); }
  } else {
    oled.setTextSize(1);
    oled.setCursor(2, 40);
    oled.print("Weather unavailable");
  }
}

static void renderSystemInfo() {
  oled.setTextSize(1);
  oled.setCursor(2, 2);   oled.print("FW: "); oled.print(FW_VERSION);
  oled.setCursor(2, 14);  oled.print("Built: "); oled.print(FW_BUILD);
  oled.setCursor(2, 28);  oled.print("Uptime: "); oled.print(formatUptime());
  oled.setCursor(2, 42);
  oled.print("WiFi: ");
  oled.print(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("disconnected"));
  oled.setCursor(2, 54);
  oled.print("Mode: "); oled.print(currentMode + 1); oled.print("/"); oled.print((int)MODE_COUNT);
}

static void renderCurrentMode() {
  oled.clearDisplay();

  struct tm t;
  bool synced = timeGetLocal(&t);

  // Spec requires the Wi-Fi indicator on every mode, system info included.
  drawWifiIndicator(WiFi.status() == WL_CONNECTED);

  if (!synced && currentMode != MODE_SYSTEM_INFO) {
    renderNoTime();
  } else {
    WeatherSnapshot w = weatherGetSnapshot();
    switch (currentMode) {
      case MODE_BIG_TIME:          renderBigTime(t); break;
      case MODE_SMALL_TIME_SEC:    renderSmallTimeSeconds(t); break;
      case MODE_TIME_DATE:         renderTimeDate(t); break;
      case MODE_TIME_DATE_WEATHER: renderTimeDateWeather(t, w); break;
      case MODE_TIME_WEATHER:      renderTimeWeather(t, w); break;
      case MODE_SYSTEM_INFO:       renderSystemInfo(); break;
      default: break;
    }
  }

  oled.display();
}

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------
void displayInit() {
  pinMode(PIN_TOUCH, INPUT);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // Nothing more we can do without the display; keep going so the rest of
    // the system (Wi-Fi, Web UI, weather) still works and is reachable.
    return;
  }
  oled.clearDisplay();
  oled.display();
  displayApplyBrightness();
  oledPoweredOn = true;
  lastModeChangeMillis = millis();
}

void displayUpdate() {
  struct tm t;
  bool synced = timeGetLocal(&t);
  bool shouldBeAwake = computeAwake(synced, t);

  if (shouldBeAwake != oledPoweredOn) {
    oledPoweredOn = shouldBeAwake;
    sendOledCommand(oledPoweredOn ? 0xAF : 0xAE); // SSD1306 display ON / OFF
    if (oledPoweredOn) displayApplyBrightness();
  }

  if (!oledPoweredOn) return;

  if (settings.autoRotate &&
      (millis() - lastModeChangeMillis) > (unsigned long)settings.rotateIntervalSec * 1000UL) {
    currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
    lastModeChangeMillis = millis();
  }

  if (millis() - lastRenderMillis >= RENDER_INTERVAL_MS) {
    lastRenderMillis = millis();
    renderCurrentMode();
  }
}
