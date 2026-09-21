# esp32NTP_weather_clock
a clock for my bathroom
# ESP32-C3 OLED Clock & Weather Display — Project Brief

## Goal

Build a simple, reliable clock and weather display using:

- ESP32-C3
- 1.3" 128×64 I2C/IIC monochrome OLED
- Touch sensor for display-mode changes and screen on/off
- Wi-Fi
- Open-Meteo weather API
- PlatformIO for compiling and uploading

Default weather ZIP code: **48842**

The project should favor reliability and simplicity over adding lots of features.

## Core Requirements

### Display modes

Provide several simple display modes:

1. Large time with blinking colon
2. Small time with seconds
3. Time + date
4. Time + date + weather
5. Time + weather
6. System/info screen included in rotation, showing:
   - Firmware version
   - Firmware build date/time
   - Uptime

Weather display should remain simple:
- Small weather icon
- Temperature
- Current condition

Every display mode should include a small Wi-Fi status indicator.

### Automatic display rotation

- Auto-rotation can be enabled/disabled from the Web UI.
- Rotation interval is configurable from the Web UI.
- Touch can manually change to the next display mode.

### Brightness

- OLED brightness is configurable from the Web UI.
- Include scheduled screen brightness/off behavior as part of the display settings.

### Screen control

Support:
- Scheduled screen OFF/ON times
- Manual screen OFF from the Web UI
- Manual screen OFF using the touch sensor
- Touch should be able to wake the display again

Turning the display off must not stop timekeeping, Wi-Fi, weather handling, or other background tasks.

## Time

- Use NTP for time synchronization.
- NTP synchronization occurs at startup and every hour while Wi-Fi is connected.
- Support 12-hour and 24-hour display formats.
- Support selectable US mainland time zones.
- If Wi-Fi is unavailable, continue displaying the last successfully synchronized time.
- Losing Wi-Fi must never stop or block the display.

## Weather

Use **Open-Meteo** as the weather service. Open-Meteo provides a no-API-key forecast API for non-commercial use. citeturn0search3turn0search11

Weather configuration:
- Default ZIP: `48842`
- Weather refresh interval selectable from the Web UI
- Temperature unit: °F / °C
- Show last successful weather update
- Show a countdown to the next weather synchronization, in minutes
- Keep the last valid weather data if a weather request fails
- Clearly distinguish stale/fallback weather from successfully refreshed weather when practical

The device must continue displaying the last valid weather data while offline.

ZIP-code handling should resolve the ZIP code to coordinates before requesting weather from Open-Meteo, since the forecast API uses latitude/longitude. citeturn0search6

## Critical Reliability Requirement

**Wi-Fi, time synchronization, weather requests, and display updates must be non-blocking.**

A weather request must never freeze the clock or prevent the display from updating.

Similarly:
- Wi-Fi connection/reconnection must not block the display.
- NTP synchronization must not block the display.
- OLED updates must not prevent network maintenance.
- Failed network requests must time out and return control to the main program.
- The main loop should remain responsive at all times.

Use timer/state-machine style scheduling rather than long blocking delays.

Expected behavior:

`Wi-Fi fails → clock continues → last weather remains → display continues`

`Wi-Fi reconnects → NTP sync → weather refresh → normal operation`

## Web UI

Provide a simple built-in Web UI for configuration.

Settings should include:

- Wi-Fi configuration / Wi-Fi Manager
- ZIP code
- Weather refresh interval
- Temperature unit
- 12/24-hour format
- US mainland timezone
- Automatic display rotation ON/OFF
- Display rotation interval
- OLED brightness
- Screen ON/OFF schedule
- Manual screen ON/OFF
- Current firmware version
- Firmware build date/time
- Uptime
- Last successful weather update
- Next weather synchronization countdown
- Wi-Fi status
- Factory reset

### Firmware update

Provide a Web UI firmware update page where a compiled `.bin` file can be uploaded.
add simple authentication for firmware update, use 1234 as placeholder, configurable in config file.

After a successful update:
- Reboot the device
- Preserve user configuration
- Show the new firmware version/build information

## Configuration Persistence

User settings must survive reboot.

Keep configuration storage simple. Do not create an elaborate configuration framework unless required.

Factory reset should restore default settings, including:

- ZIP `48842`
- Default weather settings
- Default display settings
- Default time format/timezone
- Wi-Fi configuration reset as appropriate

## Project Structure

Do **not** create a large number of source files.

Use a small structure centered around:

- `config` — settings, defaults, persistence, configuration handling
- `display` — OLED rendering and display modes
- `time` — NTP, timezone, clock/time handling
- `weather` — Open-Meteo requests, parsing, caching, refresh timing
- `main` — setup, loop, coordination, Wi-Fi/Web UI where appropriate

Additional files should only be created when they provide a clear benefit.

Use PlatformIO for compiling and uploading. PlatformIO supports ESP32-C3 boards with the Arduino framework. citeturn0search0turn0search1

## Development in 4 Testable Phases

### Phase 1 — Hardware + Basic Clock

Goal: establish a reliable display and clock foundation.

Implement:
- ESP32-C3 PlatformIO project
- OLED initialization
- Basic display rendering
- Touch sensor input
- Wi-Fi connection
- NTP synchronization
- Timezone handling
- 12/24-hour format
- Basic display modes
- Wi-Fi indicator

**Test before proceeding:**
- OLED works reliably
- Touch changes display mode
- Clock remains responsive
- Wi-Fi connects/reconnects
- NTP time is correct
- Display continues operating when Wi-Fi is unavailable

### Phase 2 — Settings + Web UI

Implement:
- Configuration persistence
- Wi-Fi Manager
- Web UI
- Time format
- Timezone
- Brightness
- Display rotation
- Rotation interval
- Screen schedule
- Manual screen ON/OFF
- Factory reset

**Test before proceeding:**
- Settings survive reboot
- Web UI changes are reflected on the display
- Factory reset works
- Screen scheduling works
- Display continues operating normally while Web UI is being used

### Phase 3 — Weather + Offline/Fallback Behavior

Implement:
- Open-Meteo integration
- ZIP-to-coordinate lookup
- Weather refresh interval
- Temperature unit
- Weather icon/condition
- Last successful update
- Next sync countdown
- Cached weather fallback
- Weather error handling

**Test before proceeding:**
- Weather loads successfully
- Weather refreshes at the configured interval
- Failed weather requests do not freeze the display
- Wi-Fi loss does not stop the clock
- Cached weather remains available offline
- Reconnection restores normal synchronization

### Phase 4 — OTA + Polish + Reliability

Implement:
- Web UI `.bin` firmware upload
- Firmware version/build information
- Uptime
- System/info display mode
- Final display rotation behavior
- Screen scheduling polish
- Network timeout/retry handling
- Reboot/error diagnostics as useful
- Final cleanup

**Test before release:**
- OTA update works
- Settings survive OTA update
- Wi-Fi loss/recovery works
- Weather API failure works
- NTP failure works
- Display never becomes permanently blocked
- Touch remains responsive
- Screen scheduling works
- Long-duration operation is stable

## AI Code Development Guidelines

When modifying the project:

1. **Keep it simple.** Do not introduce frameworks or abstractions unless they solve a real problem.
2. **Do not create unnecessary files.** Prefer the existing `config`, `display`, `time`, `weather`, and main structure.
3. **Make one logical change at a time.**
4. **Compile after meaningful changes.**
5. **Do not change working behavior unnecessarily.**
6. **Preserve existing variable names and interfaces when practical.**
7. **Avoid blocking code**, especially long `delay()` calls, blocking Wi-Fi loops, blocking weather requests, or blocking NTP operations.
8. **Every network operation needs failure handling and a timeout.**
9. **Never allow weather or Wi-Fi code to prevent display updates.**
10. **Keep cached/last-known data available when offline.**
11. **Do not invent hardware pins or hardware capabilities.** Ask if information is missing.
12. **Do not add features outside the current phase unless explicitly requested.**
13. **Keep debugging output useful and concise.**
14. **Before changing architecture, explain why the change is necessary.**
15. **Each phase must remain independently testable.**
16. **When fixing a bug, identify the cause before rewriting unrelated code.**

## Design Principle

The device is primarily a **clock**.

Weather, Wi-Fi, Web UI, and OTA are supporting features.

Therefore:

**Clock/display operation must continue even when Wi-Fi, NTP, Open-Meteo, or the Web UI is unavailable.**

The system should degrade gracefully rather than stop functioning.
