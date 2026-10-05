#pragma once
#include <stdint.h>

// Wall-clock helpers for day/night and seasonal hats. Pure (no Arduino, no time.h) so the
// PC preview and tests use the same code. The board has no RTC: local time comes from GPS
// (UTC + the owner's offset) or is set by hand in Setup and lost at power-off. Times here
// are "local unix seconds": seconds since 1970-01-01 00:00 in local time.
namespace clk {

struct Civil {
  uint16_t year;
  uint8_t month;   // 1-12
  uint8_t day;     // 1-31
  uint8_t hour, minute;
};

// Days since 1970-01-01 <-> date (Howard Hinnant's algorithm, valid far beyond 2106).
inline int32_t daysFromCivil(int32_t y, uint32_t m, uint32_t d) {
  y -= m <= 2;
  int32_t era = (y >= 0 ? y : y - 399) / 400;
  uint32_t yoe = (uint32_t)(y - era * 400);
  uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

inline Civil fromUnix(uint32_t t) {
  int32_t z = (int32_t)(t / 86400) + 719468;
  uint32_t secs = t % 86400;
  int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  uint32_t doe = (uint32_t)(z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t mp = (5 * doy + 2) / 153;
  Civil c;
  c.day = (uint8_t)(doy - (153 * mp + 2) / 5 + 1);
  c.month = (uint8_t)(mp < 10 ? mp + 3 : mp - 9);
  c.year = (uint16_t)((int32_t)yoe + era * 400 + (c.month <= 2));
  c.hour = (uint8_t)(secs / 3600);
  c.minute = (uint8_t)(secs / 60 % 60);
  return c;
}

inline uint32_t toUnix(const Civil& c) {
  return (uint32_t)daysFromCivil(c.year, c.month, c.day) * 86400u + c.hour * 3600u + c.minute * 60u;
}

inline uint8_t daysInMonth(uint16_t y, uint8_t m) {
  static const uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  return (uint8_t)(m == 2 && leap ? 29 : kDays[(m - 1) % 12]);
}

// Times of day for the background (hours, local).
enum Phase : uint8_t { kDay, kDawn, kDusk, kNight };
inline Phase phase(uint8_t hour) {
  if (hour >= 21 || hour < 5) return kNight;
  if (hour < 8) return kDawn;
  if (hour >= 18) return kDusk;
  return kDay;
}
// The goblin naps from 23:00 to 06:00 (scanning carries on).
inline bool napTime(uint8_t hour) { return hour >= 23 || hour < 6; }

}  // namespace clk
