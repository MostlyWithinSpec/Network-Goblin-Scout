#pragma once
#include <Arduino.h>

// Optional LiPo fuel gauge on CN1, see docs/battery.md: MAX17048 (I2C 0x36) or BQ27441
// (0x55, SparkFun Battery Babysitter). Without one, present() stays false and nothing
// battery-related is shown.
namespace battery {
bool begin();          // true if a gauge answered
bool present();
const char* name();     // "MAX17048", "BQ27441" or "none"
void poll();           // cheap; reads the gauge every few seconds
uint8_t percent();     // 0..100
uint16_t millivolts();
bool charging();       // the cell is gaining charge
bool discharging();    // running on the battery
}  // namespace battery
