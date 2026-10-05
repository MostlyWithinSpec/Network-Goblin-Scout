// PC unit tests for core/Clock.h:
//   g++ -std=c++17 -Wall -Wextra -I src test/test_clock.cpp -o /tmp/t && /tmp/t
#include <cstdio>
#include <ctime>
#include "core/Clock.h"

static int fails = 0;
#define CHECK(c)                                          \
  do {                                                    \
    if (!(c)) {                                           \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); \
      fails++;                                            \
    }                                                     \
  } while (0)

int main() {
  clk::Civil c = clk::fromUnix(0);
  CHECK(c.year == 1970 && c.month == 1 && c.day == 1 && c.hour == 0 && c.minute == 0);
  // compare with the C library over 2000-2100, every ~13 hours 37 minutes
  for (uint32_t t = 946684800u; t < 4102444800u; t += 49020u) {
    time_t tt = (time_t)t;
    struct tm g;
    gmtime_r(&tt, &g);
    clk::Civil x = clk::fromUnix(t);
    CHECK(x.year == g.tm_year + 1900 && x.month == g.tm_mon + 1 && x.day == g.tm_mday && x.hour == g.tm_hour &&
          x.minute == g.tm_min);
    CHECK(clk::toUnix(x) == t - t % 60);
    if (fails > 5) break;
  }
  CHECK(clk::daysInMonth(2024, 2) == 29 && clk::daysInMonth(2026, 2) == 28 && clk::daysInMonth(2100, 2) == 28);
  CHECK(clk::daysInMonth(2000, 2) == 29 && clk::daysInMonth(2026, 12) == 31);
  CHECK(clk::phase(12) == clk::kDay && clk::phase(6) == clk::kDawn && clk::phase(19) == clk::kDusk);
  CHECK(clk::phase(23) == clk::kNight && clk::phase(2) == clk::kNight);
  CHECK(clk::napTime(23) && clk::napTime(5) && !clk::napTime(6) && !clk::napTime(22));
  if (fails) {
    printf("%d clock test(s) failed\n", fails);
    return 1;
  }
  printf("all clock tests passed\n");
  return 0;
}
