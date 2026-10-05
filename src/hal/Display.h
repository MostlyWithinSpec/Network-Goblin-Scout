#pragma once
#include <Arduino_GFX_Library.h>

// Double-buffered display: everything draws into a PSRAM canvas, flush() pushes it.
namespace display {
bool begin();
Arduino_Canvas* gfx();
uint16_t* framebuffer();            // 320x240 RGB565 in PSRAM (null if init failed)
void flush();
void setBrightness(uint8_t pct);   // 0..100
void setInverted(bool inv);
void setFast(bool on);             // "Turbo display": full crystal-speed SPI
void sleep(bool on);               // backlight off, panel stays initialised
bool asleep();
}  // namespace display
