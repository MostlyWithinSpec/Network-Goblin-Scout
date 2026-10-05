#pragma once
#include <Arduino.h>

// Small feedback helpers: RGB LED blips and speaker chirps/jingles, all non-blocking.
namespace fx {
struct Note {
  uint16_t freq;  // Hz, 0 = rest
  uint16_t ms;
};

void begin();
void update();                                   // call every loop
void led(uint8_t r, uint8_t g, uint8_t b, uint16_t ms);
void chirp(uint16_t freq, uint16_t ms);
void play(const Note* notes, uint8_t count);     // replaces whatever is playing
void setSound(bool on);
}  // namespace fx
