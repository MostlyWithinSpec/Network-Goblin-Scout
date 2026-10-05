#pragma once
#include <Arduino.h>

// Optional MAX17048 LiPo fuel gauge on CN1 (I2C 0x36), see docs/battery.md.
// Without one, present() stays false and nothing battery-related is shown.
namespace battery {
bool begin();          // true if a gauge answered
bool present();
void poll();           // cheap; reads the gauge every few seconds
uint8_t percent();     // 0..100
uint16_t millivolts();
bool charging();       // the cell is gaining charge
bool discharging();    // running on the battery
}  // namespace battery
