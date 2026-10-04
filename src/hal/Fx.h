#pragma once
#include <Arduino.h>

// Small feedback helpers: RGB LED blips and speaker chirps, both non-blocking.
namespace fx {
void begin();
void update();                                   // call every loop
void led(uint8_t r, uint8_t g, uint8_t b, uint16_t ms);
void chirp(uint16_t freq, uint16_t ms);
void setSound(bool on);
}  // namespace fx
