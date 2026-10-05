#include "Fx.h"
#include "board.h"

namespace {
uint32_t ledOffAt = 0;
uint32_t toneOffAt = 0;
bool soundOn = true;

const uint8_t kMaxNotes = 8;
fx::Note queue[kMaxNotes];
uint8_t qLen = 0, qPos = 0;

void startTone(uint16_t freq, uint16_t ms) {
  ledcWriteTone(PIN_SPEAKER, freq);
  toneOffAt = millis() + ms;
  if (!toneOffAt) toneOffAt = 1;
}
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
    toneOffAt = 0;
    if (qPos < qLen) {
      const Note& n = queue[qPos++];
      startTone(n.freq, n.ms);
    } else {
      ledcWriteTone(PIN_SPEAKER, 0);
    }
  }
}

void led(uint8_t r, uint8_t g, uint8_t b, uint16_t ms) {
  rgbLedWrite(PIN_RGB_LED, r, g, b);
  ledOffAt = millis() + ms;
}

void chirp(uint16_t freq, uint16_t ms) {
  if (!soundOn) return;
  qLen = qPos = 0;
  startTone(freq, ms);
}

void play(const Note* notes, uint8_t count) {
  if (!soundOn || !count) return;
  if (count > kMaxNotes) count = kMaxNotes;
  memcpy(queue, notes, count * sizeof(Note));
  qLen = count;
  qPos = 1;
  startTone(queue[0].freq, queue[0].ms);
}

void setSound(bool on) {
  soundOn = on;
  if (!on) {
    qLen = qPos = 0;
    ledcWriteTone(PIN_SPEAKER, 0);
  }
}

}  // namespace fx
