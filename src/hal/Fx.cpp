#include "Fx.h"
#include "board.h"

namespace {
uint32_t ledOffAt = 0;
uint32_t toneOffAt = 0;
bool soundOn = true;
}  // namespace

namespace fx {

void begin() {
  rgbLedWrite(PIN_RGB_LED, 0, 0, 0);
  ledcAttach(PIN_SPEAKER, 2000, 10);
  ledcWriteTone(PIN_SPEAKER, 0);
}

void update() {
  uint32_t now = millis();
  if (ledOffAt && (int32_t)(now - ledOffAt) >= 0) {
    rgbLedWrite(PIN_RGB_LED, 0, 0, 0);
    ledOffAt = 0;
  }
  if (toneOffAt && (int32_t)(now - toneOffAt) >= 0) {
    ledcWriteTone(PIN_SPEAKER, 0);
    toneOffAt = 0;
  }
}

void led(uint8_t r, uint8_t g, uint8_t b, uint16_t ms) {
  rgbLedWrite(PIN_RGB_LED, r, g, b);
  ledOffAt = millis() + ms;
}

void chirp(uint16_t freq, uint16_t ms) {
  if (!soundOn) return;
  ledcWriteTone(PIN_SPEAKER, freq);
  toneOffAt = millis() + ms;
}

void setSound(bool on) {
  soundOn = on;
  if (!on) ledcWriteTone(PIN_SPEAKER, 0);
}

}  // namespace fx
