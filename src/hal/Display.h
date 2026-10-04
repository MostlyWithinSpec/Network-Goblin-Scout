#pragma once
#include <Arduino_GFX_Library.h>

// Double-buffered display: everything draws into a PSRAM canvas, flush() pushes it.
namespace display {
bool begin();
Arduino_Canvas* gfx();
void flush();
void setBrightness(uint8_t pct);   // 0..100
void setInverted(bool inv);
void sleep(bool on);               // backlight off, panel stays initialised
bool asleep();
}  // namespace display
