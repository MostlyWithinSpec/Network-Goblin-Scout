// Host unit test for quests and hats. Build & run (one line):
//   g++ -std=c++17 -Wall -Wextra -I tools/preview/shim -I src test/test_quests.cpp src/core/Quests.cpp src/core/Hats.cpp -o /tmp/tq && /tmp/tq
#include <cstdio>
#include <cstring>
#include "core/Hats.h"
#include "core/Quests.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main() {
  Stats s;
  s.wifiUnique = 50;
  // A fresh goblin: never heard mesh, never met a goblin, no GPS -> those quests are never dealt.
  for (uint32_t seed = 1; seed < 500; seed++) {
    QuestBoard b;
    quests::deal(s, 1, seed, b);
    CHECK(b.active);
    for (int i = 0; i < 3; i++) {
      CHECK(b.q[i].type < quests::kTypeCount);
      CHECK(b.q[i].type != quests::kNewMesh && b.q[i].type != quests::kMeetGoblin && b.q[i].type != quests::kNewArea);
      CHECK(b.q[i].target >= 1);
      for (int j = 0; j < i; j++) CHECK(b.q[i].type != b.q[j].type);
    }
  }
  // Progress counts only growth since the board was dealt, capped at the target.
  Quest q;
  q.type = quests::kNewWifi;
  q.target = 10;
  q.base = s.wifiUnique;
  CHECK(quests::progress(s, q) == 0);
  s.wifiUnique += 4;
  CHECK(quests::progress(s, q) == 4);
  s.wifiUnique += 40;
  CHECK(quests::progress(s, q) == 10);
  CHECK(quests::reward(q) > 0);
  char d[48];
  quests::describe(q, d, sizeof(d));
  CHECK(strcmp(d, "Sniff out 10 new networks") == 0);
  q.target = 1;
  quests::describe(q, d, sizeof(d));
  CHECK(strcmp(d, "Sniff out a new network") == 0);

  // Hats
  Stats h;
  CHECK(hats::unlocked(0, h, 1));
  CHECK(!hats::unlocked(2, h, 4));
  CHECK(hats::unlocked(2, h, 5));          // Antenna Band at level 5
  CHECK(!hats::unlocked(hats::kCount + 1, h, 99));
  h.questsDone = 1;
  CHECK(hats::unlockedMask(h, 1) & 1u);    // Party Hat

  printf(failures ? "%d FAILED\n" : "all quest/hat tests passed\n", failures);
  return failures != 0;
}
