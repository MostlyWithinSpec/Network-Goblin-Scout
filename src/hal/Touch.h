#pragma once
#include <Arduino.h>

// Minimal XPT2046 driver on the shared SPI bus (no library needed).
namespace touch {
void begin();
// Returns true while the panel is pressed; x/y in screen coordinates.
bool read(int16_t& x, int16_t& y);
// Last raw ADC readings (for the calibration screen).
void lastRaw(uint16_t& rx, uint16_t& ry, uint16_t& z);
}  // namespace touch
