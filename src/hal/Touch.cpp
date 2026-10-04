#include "Touch.h"
#include <SPI.h>
#include "board.h"
#include "config.h"

namespace {
const SPISettings kTouchSpi(TOUCH_SPI_HZ, MSBFIRST, SPI_MODE0);
uint16_t rawX = 0, rawY = 0, rawZ = 0;

uint16_t cmd12(uint8_t cmd) {
  SPI.transfer(cmd);
  return SPI.transfer16(0) >> 3;  // 12-bit result, left-aligned
}

int16_t mapClamp(int32_t v, int32_t inMin, int32_t inMax, int32_t outMax) {
  int32_t o = (v - inMin) * outMax / (inMax - inMin);
  if (o < 0) o = 0;
  if (o > outMax - 1) o = outMax - 1;
  return (int16_t)o;
}
}  // namespace

namespace touch {

void begin() {
  pinMode(PIN_TOUCH_CS, OUTPUT);
  digitalWrite(PIN_TOUCH_CS, HIGH);
  pinMode(PIN_TOUCH_IRQ, INPUT_PULLUP);
}

bool read(int16_t& x, int16_t& y) {
  if (digitalRead(PIN_TOUCH_IRQ) == HIGH) return false;  // not pressed

  SPI.beginTransaction(kTouchSpi);
  digitalWrite(PIN_TOUCH_CS, LOW);

  uint16_t z1 = cmd12(0xB1);
  uint16_t z2 = cmd12(0xC1);
  int32_t z = (int32_t)z1 + 4095 - (int32_t)z2;

  uint32_t sx = 0, sy = 0;
  cmd12(0xD1);  // discard first conversion (settling)
  for (int i = 0; i < 4; i++) {
    sx += cmd12(0xD1);
    sy += cmd12(0x91);
  }
  SPI.transfer(0x80);  // power down, keep PENIRQ enabled
  SPI.transfer16(0);

  digitalWrite(PIN_TOUCH_CS, HIGH);
  SPI.endTransaction();

  rawX = sx / 4;
  rawY = sy / 4;
  rawZ = z < 0 ? 0 : (uint16_t)z;
  if (z < TOUCH_MIN_PRESSURE) return false;

  int32_t ax = rawX, ay = rawY;
#if TOUCH_SWAP_XY
  int32_t t = ax; ax = ay; ay = t;
#endif
  x = mapClamp(ax, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX, SCREEN_W);
  y = mapClamp(ay, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX, SCREEN_H);
#if TOUCH_INVERT_X
  x = SCREEN_W - 1 - x;
#endif
#if TOUCH_INVERT_Y
  y = SCREEN_H - 1 - y;
#endif
  return true;
}

void lastRaw(uint16_t& rx, uint16_t& ry, uint16_t& z) {
  rx = rawX; ry = rawY; z = rawZ;
}

}  // namespace touch
