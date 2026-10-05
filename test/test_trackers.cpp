// PC unit tests for core/Trackers (no board needed):
//   g++ -std=c++17 -Wall -Wextra -I src test/test_trackers.cpp src/core/Trackers.cpp -o /tmp/t && /tmp/t
#include <cstdio>
#include <cstdlib>
#include "core/Trackers.h"

using namespace trackers;

static int fails = 0;
#define CHECK(c)                                              \
  do {                                                        \
    if (!(c)) {                                               \
      printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);     \
      fails++;                                                \
    }                                                         \
  } while (0)

const uint32_t kMin = 60000;

static void places() {
  PlaceTracker p;
  uint64_t home[] = {1, 2, 3, 4};
  p.wifiScan(home, 4);
  uint32_t a = p.place();
  uint64_t homeAgain[] = {3, 9, 10};  // one of the strongest is still there
  p.wifiScan(homeAgain, 3);
  CHECK(p.place() == a);
  uint64_t quiet[] = {42};
  p.wifiScan(quiet, 1);  // too few networks to judge
  CHECK(p.place() == a);
  uint64_t street[] = {20, 21, 22};
  p.wifiScan(street, 3);
  CHECK(p.place() == a + 1);
  p.gpsCell(77);  // first fix only sets the cell
  CHECK(p.place() == a + 1);
  p.gpsCell(77);
  CHECK(p.place() == a + 1);
  p.gpsCell(78);
  CHECK(p.place() == a + 2);
}

static void follows() {
  Watch w;
  uint32_t t = 1000;
  // Same tracker, 3 places, but only 10 minutes: no alert yet.
  CHECK(!w.see(5, kFindMy, -60, 1, t));
  CHECK(!w.see(5, kFindMy, -60, 2, t + 5 * kMin));
  CHECK(!w.see(5, kFindMy, -60, 3, t + 10 * kMin));
  // Still with us after 15 minutes: alert, exactly once.
  const Follower* f = w.see(5, kFindMy, -55, 3, t + 15 * kMin);
  CHECK(f && f->kind == kFindMy && f->places == 3);
  CHECK(!w.see(5, kFindMy, -55, 4, t + 16 * kMin));
  CHECK(w.nearby(t + 16 * kMin) == 1);
}

static void staysPut() {
  Watch w;
  // A tracker at home all day (one place): never alerts.
  for (uint32_t m = 0; m < 600; m += 5) CHECK(!w.see(6, kTile, -70, 1, 1000 + m * kMin));
}

static void leftBehind() {
  Watch w;
  uint32_t t = 1000;
  w.see(7, kTile, -70, 1, t);
  w.see(7, kTile, -70, 2, t + 5 * kMin);
  // Not heard for 30 minutes (we walked away), then a tracker with the same id shows up
  // somewhere else: the history restarted, so no alert.
  CHECK(!w.see(7, kTile, -70, 3, t + 35 * kMin));
  CHECK(!w.see(7, kTile, -70, 4, t + 40 * kMin));
}

static void mine() {
  Watch w;
  uint32_t t = 1000;
  w.see(8, kGoogle, -50, 1, t);
  w.see(8, kGoogle, -50, 2, t + 8 * kMin);
  w.forget(8);
  CHECK(!w.see(8, kGoogle, -50, 3, t + 16 * kMin));  // starts over after "it's mine"
}

static void crowded() {
  Watch w;
  // A busy station: 3 new passing trackers every minute, far more than there are slots.
  // The one that keeps following us must not be pushed out by them.
  uint32_t t = 1000;
  const Follower* hit = nullptr;
  uint64_t passing = 1000;
  for (uint32_t m = 0; m <= 20; m++) {
    for (int k = 0; k < 3; k++) w.see(passing++, kFindMy, -80, 1 + m / 5, t + m * kMin);
    const Follower* f = w.see(9, kFindMy, -50, 1 + m / 5, t + m * kMin + 1);
    if (f) hit = f;
  }
  CHECK(hit && hit->id == 9);
}

int main() {
  places();
  follows();
  staysPut();
  leftBehind();
  mine();
  crowded();
  if (fails) {
    printf("%d tracker test(s) failed\n", fails);
    return 1;
  }
  printf("all tracker tests passed\n");
  return 0;
}
