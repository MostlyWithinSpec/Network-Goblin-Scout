#pragma once
#include <Arduino.h>
#include <Wire.h>

// Minimal AHT20 temperature/humidity driver (on-board, I2C address 0x38).
// Non-blocking: start() a measurement, then call read() >= 80 ms later.
namespace aht20 {
const uint8_t kAddr = 0x38;
bool begin(TwoWire& wire);           // true if a sensor answered at 0x38
bool present();
bool start();                        // trigger a measurement
bool read(float& tempC, float& rh);  // false while busy or on CRC error
}  // namespace aht20
