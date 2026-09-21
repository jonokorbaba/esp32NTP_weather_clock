#pragma once
// ---------------------------------------------------------------------------
// Hardware wiring for the classic ESP32 build.
// If you rewire anything, this is the only place that should need editing.
// ---------------------------------------------------------------------------

#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define PIN_TOUCH   18   // TTP223 module OUT pin (digital HIGH while touched)

#define OLED_ADDR      0x3C
#define OLED_WIDTH     128
#define OLED_HEIGHT    64
