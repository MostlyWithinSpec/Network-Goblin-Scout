#include "Aht20.h"

// Protocol from the AHT20 datasheet (Aosong, v1.1): status 0x71, init 0xBE 0x08 0x00,
// trigger 0xAC 0x33 0x00, then 7 bytes back: status, 20-bit RH, 20-bit T, CRC-8.
namespace {
TwoWire* bus = nullptr;
bool found = false;

uint8_t status() {
  bus->requestFrom(aht20::kAddr, (uint8_t)1);
  return bus->available() ? bus->read() : 0xFF;
}

bool send3(uint8_t a, uint8_t b, uint8_t c) {
  bus->beginTransmission(aht20::kAddr);
  bus->write(a);
  bus->write(b);
  bus->write(c);
  return bus->endTransmission() == 0;
}

uint8_t crc8(const uint8_t* d, size_t n) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= d[i];
    for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}
}  // namespace

namespace aht20 {

bool begin(TwoWire& wire) {
  bus = &wire;
  found = false;
  bus->beginTransmission(kAddr);
  if (bus->endTransmission() != 0) return false;
  if (!(status() & 0x08)) {  // not calibrated yet: send init
    send3(0xBE, 0x08, 0x00);
    delay(10);
  }
  found = (status() & 0x08) != 0;
  return found;
}

bool present() { return found; }

bool start() { return found && send3(0xAC, 0x33, 0x00); }

bool read(float& tempC, float& rh) {
  if (!found) return false;
  uint8_t d[7];
  if (bus->requestFrom(kAddr, (uint8_t)7) != 7) return false;
  for (auto& b : d) b = bus->read();
  if (d[0] & 0x80) return false;  // still measuring
  if (crc8(d, 6) != d[6]) return false;
  uint32_t rawH = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
  uint32_t rawT = ((uint32_t)(d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];
  rh = rawH * 100.0f / 1048576.0f;
  tempC = rawT * 200.0f / 1048576.0f - 50.0f;
  return true;
}

}  // namespace aht20
