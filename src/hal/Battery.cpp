#include "Battery.h"
#include <Wire.h>
#include "board.h"

// Registers from the MAX17048 datasheet: VCELL 0x02 (78.125 uV/LSB), SOC 0x04 (1/256 %),
// VERSION 0x08 (0x001x), CRATE 0x16 (signed, 0.208 %/hour per LSB).
namespace {
const uint8_t kAddr = 0x36;
bool found = false;
uint8_t pct = 0;
uint16_t mv = 0;
int16_t rate = 0;   // CRATE, 0.208 %/hour per LSB
uint32_t lastPollMs = 0;

bool read16(uint8_t reg, uint16_t& out) {
  Wire.beginTransmission(kAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(kAddr, (uint8_t)2) != 2) return false;
  out = (uint16_t)(Wire.read() << 8);
  out |= (uint16_t)Wire.read();
  return true;
}

void sample() {
  uint16_t v, soc, crate;
  if (!read16(0x02, v) || !read16(0x04, soc)) return;
  mv = (uint16_t)((uint32_t)v * 78125 / 1000000);
  uint16_t p = soc >> 8;
  pct = (uint8_t)(p > 100 ? 100 : p);
  rate = read16(0x16, crate) ? (int16_t)crate : 0;
}
}  // namespace

namespace battery {

bool begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  uint16_t ver = 0;
  found = read16(0x08, ver) && (ver & 0xFFF0) == 0x0010;
  if (found) {
    sample();
    log_i("battery: MAX17048 found, %u%%, %u mV", pct, mv);
  } else {
    log_i("battery: no fuel gauge on CN1 (USB power only)");
  }
  return found;
}

bool present() { return found; }

void poll() {
  if (!found || millis() - lastPollMs < 5000) return;
  lastPollMs = millis();
  sample();
}

uint8_t percent() { return pct; }
uint16_t millivolts() { return mv; }
// More than ~1 %/hour (5 LSB) either way; a full cell resting on USB reads about 0.
bool charging() { return rate > 5; }
bool discharging() { return rate < -5; }

}  // namespace battery
