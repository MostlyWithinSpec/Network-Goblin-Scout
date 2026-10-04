#include "Gps.h"
#include <TinyGPSPlus.h>
#include "board.h"
#include "config.h"

namespace {
TinyGPSPlus parser;
HardwareSerial& port = Serial1;
bool started = false;
uint32_t lastValidSentenceMs = 0;
uint32_t lastPassed = 0;

// days since 1970-01-01 for a civil date (Howard Hinnant's algorithm)
int32_t daysFromCivil(int32_t y, uint32_t m, uint32_t d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = (uint32_t)(y - era * 400);
  const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}
}  // namespace

namespace gps {

void begin() {
  port.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  started = true;
}

void poll() {
  if (!started) return;
  while (port.available()) parser.encode((char)port.read());
  if (parser.passedChecksum() != lastPassed) {
    lastPassed = parser.passedChecksum();
    lastValidSentenceMs = millis();
  }
}

bool present() { return started && lastValidSentenceMs && millis() - lastValidSentenceMs < 5000; }
bool hasFix() { return present() && parser.location.isValid() && parser.location.age() < 5000; }
double lat() { return parser.location.lat(); }
double lon() { return parser.location.lng(); }
uint8_t satellites() { return parser.satellites.isValid() ? parser.satellites.value() : 0; }

bool timeValid() {
  return present() && parser.date.isValid() && parser.time.isValid() && parser.date.year() >= 2024;
}

uint32_t unixTime() {
  if (!timeValid()) return 0;
  int32_t days = daysFromCivil(parser.date.year(), parser.date.month(), parser.date.day());
  return (uint32_t)days * 86400UL + parser.time.hour() * 3600UL + parser.time.minute() * 60UL +
         parser.time.second();
}

}  // namespace gps
