#include "Battery.h"
#include <Wire.h>
#include "board.h"
#include "config.h"

// Two supported fuel gauges, detected at boot on CN1:
//
// MAX17048 (0x36), e.g. Adafruit #5580. Big-endian registers: VCELL 0x02 (78.125 uV/LSB),
//   SOC 0x04 (1/256 %), VERSION 0x08 (0x001x), CRATE 0x16 (signed, 0.208 %/hour per LSB).
//
// BQ27441-G1 (0x55), e.g. SparkFun Battery Babysitter. Little-endian "standard commands":
//   Control 0x00, Voltage 0x04 (mV), Flags 0x06, AverageCurrent 0x10 (signed mA, + = charging),
//   StateOfCharge 0x1C (%), DesignCapacity 0x3C (mAh). It counts charge in and out, so it needs
//   to know the cell's capacity: we write BATTERY_CAPACITY_MAH into its data memory when it
//   differs (sequence from TI's BQ27441-G1 Technical Reference Manual, section "Data memory").
//   That setting lives in the gauge's RAM and survives our reboots, but not the battery being
//   unplugged, hence the check at every boot.
namespace {
enum class Chip : uint8_t { None, Max17048, Bq27441 };
Chip chip = Chip::None;
uint8_t pct = 0;
uint16_t mv = 0;
int16_t rate = 0;   // MAX17048: CRATE units; BQ27441: mA
uint32_t lastPollMs = 0;

const uint8_t kMax = 0x36, kBq = 0x55;

// ---- MAX17048 -----------------------------------------------------------------
bool maxRead(uint8_t reg, uint16_t& out) {
  Wire.beginTransmission(kMax);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(kMax, (uint8_t)2) != 2) return false;
  out = (uint16_t)(Wire.read() << 8);
  out |= (uint16_t)Wire.read();
  return true;
}

// ---- BQ27441 ------------------------------------------------------------------
bool bqRead(uint8_t reg, uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(kBq);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(kBq, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = (uint8_t)Wire.read();
  return true;
}

bool bqRead16(uint8_t reg, uint16_t& out) {
  uint8_t b[2];
  if (!bqRead(reg, b, 2)) return false;
  out = (uint16_t)(b[0] | b[1] << 8);
  return true;
}

bool bqWrite(uint8_t reg, const uint8_t* buf, uint8_t n) {
  Wire.beginTransmission(kBq);
  Wire.write(reg);
  for (uint8_t i = 0; i < n; i++) Wire.write(buf[i]);
  return Wire.endTransmission() == 0;
}

bool bqWrite8(uint8_t reg, uint8_t v) { return bqWrite(reg, &v, 1); }

bool bqControl(uint16_t sub) {
  uint8_t b[2] = {(uint8_t)sub, (uint8_t)(sub >> 8)};
  return bqWrite(0x00, b, 2);
}

bool bqControlRead(uint16_t sub, uint16_t& out) {
  if (!bqControl(sub)) return false;
  delay(2);
  return bqRead16(0x00, out);
}

bool bqWaitCfgUpdate(bool on) {  // Flags bit 4 = CFGUPMODE
  for (int i = 0; i < 100; i++) {
    uint16_t flags;
    if (bqRead16(0x06, flags) && (bool)(flags & 0x10) == on) return true;
    delay(20);
  }
  return false;
}

// Design capacity: subclass 82 (State), block 0, offset 10, big-endian in data memory.
bool bqSetCapacity(uint16_t mah) {
  uint16_t cur;
  if (bqRead16(0x3C, cur) && cur == mah) return true;
  log_i("battery: setting BQ27441 design capacity %u -> %u mAh", cur, mah);
  bool ok = bqControl(0x8000) && bqControl(0x8000);  // unseal (default key)
  ok = ok && bqControl(0x0013) && bqWaitCfgUpdate(true);  // SET_CFGUPDATE
  ok = ok && bqWrite8(0x61, 0x00) && bqWrite8(0x3E, 82) && bqWrite8(0x3F, 0);
  delay(5);
  uint8_t block[32];
  ok = ok && bqRead(0x40, block, 32);
  if (ok) {
    block[10] = (uint8_t)(mah >> 8);
    block[11] = (uint8_t)mah;
    ok = bqWrite(0x40 + 10, block + 10, 2);
    uint8_t sum = 0;
    for (uint8_t b : block) sum += b;
    ok = ok && bqWrite8(0x60, (uint8_t)(255 - sum));  // block checksum commits the write
  }
  delay(5);
  bool reset = bqControl(0x0042) && bqWaitCfgUpdate(false);  // SOFT_RESET leaves config mode
  bqControl(0x0020);                                         // seal again
  if (!ok || !reset) log_w("battery: couldn't set BQ27441 capacity, % may be off");
  return ok && reset;
}

void sample() {
  if (chip == Chip::Max17048) {
    uint16_t v, soc, crate;
    if (!maxRead(0x02, v) || !maxRead(0x04, soc)) return;
    mv = (uint16_t)((uint32_t)v * 78125 / 1000000);
    uint16_t p = soc >> 8;
    pct = (uint8_t)(p > 100 ? 100 : p);
    rate = maxRead(0x16, crate) ? (int16_t)crate : 0;
  } else if (chip == Chip::Bq27441) {
    uint16_t v, soc, cur;
    if (!bqRead16(0x04, v) || !bqRead16(0x1C, soc)) return;
    mv = v;
    pct = (uint8_t)(soc > 100 ? 100 : soc);
    rate = bqRead16(0x10, cur) ? (int16_t)cur : 0;
  }
}
}  // namespace

namespace battery {

bool begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  chip = Chip::None;
  uint16_t id = 0;
  if (maxRead(0x08, id) && (id & 0xFFF0) == 0x0010) {
    chip = Chip::Max17048;
  } else if (bqControlRead(0x0001, id) && id == 0x0421) {  // DEVICE_TYPE
    chip = Chip::Bq27441;
    bqSetCapacity(BATTERY_CAPACITY_MAH);
  }
  if (chip != Chip::None) {
    sample();
    log_i("battery: %s found, %u%%, %u mV", name(), pct, mv);
  } else {
    log_i("battery: no fuel gauge on CN1 (USB power only)");
  }
  return chip != Chip::None;
}

bool present() { return chip != Chip::None; }

const char* name() {
  switch (chip) {
    case Chip::Max17048: return "MAX17048";
    case Chip::Bq27441: return "BQ27441";
    default: return "none";
  }
}

void poll() {
  if (chip == Chip::None || millis() - lastPollMs < 5000) return;
  lastPollMs = millis();
  sample();
}

uint8_t percent() { return pct; }
uint16_t millivolts() { return mv; }
// MAX17048: more than ~1 %/hour (5 LSB) either way. BQ27441: more than 10 mA either way.
// A full cell resting on USB reads about 0 on both.
bool charging() { return chip == Chip::Bq27441 ? rate > 10 : rate > 5; }
bool discharging() { return chip == Chip::Bq27441 ? rate < -10 : rate < -5; }

}  // namespace battery
