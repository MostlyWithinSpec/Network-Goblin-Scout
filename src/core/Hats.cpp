#include "Hats.h"

namespace hats {

#define H(name, how, expr) {name, how, [](const Stats& s, uint16_t level) -> bool { (void)s; (void)level; return (expr); }}

const Def kHats[] = {
    H("Party Hat", "Finish your first quest", s.questsDone >= 1),               // 1
    H("Antenna Band", "Reach level 5", level >= 5),                              // 2
    H("Beanie", "Find 100 networks", s.wifiUnique >= 100),                       // 3
    H("Propeller Cap", "Finish 10 quests", s.questsDone >= 10),                  // 4
    H("Pirate Bandana", "Find 25 open networks", s.wifiOpen >= 25),              // 5
    H("Top Hat", "Unlock 25 achievements", s.achieved.count() >= 25),            // 6
    H("Viking Helmet", "Meet 5 goblins", s.peersMet >= 5),                       // 7
    H("Crown", "Reach level 20", level >= 20),                                   // 8
    H("Wizard Hat", "Reach level 30", level >= 30),                              // 9
    H("Halo", "Unlock 90 achievements", s.achieved.count() >= 90),               // 10
    H("Chef Hat", "Pet your goblin 100 times", s.pets >= 100),                   // 11
    H("Tinfoil Hat", "Detect 50 hidden networks", s.wifiHidden >= 50),           // 12
    H("Bee Antennae", "Find a Zigbee network", s.zigbeePans >= 1),               // 13
};

#undef H

const uint8_t kCount = sizeof(kHats) / sizeof(kHats[0]);
static_assert(sizeof(kHats) / sizeof(kHats[0]) <= 31, "hat id must fit in 5 bits (beacon)");

bool unlocked(uint8_t id, const Stats& s, uint16_t level) {
  if (id == 0) return true;
  if (id > kCount) return false;
  return kHats[id - 1].unlocked(s, level);
}

uint32_t unlockedMask(const Stats& s, uint16_t level) {
  uint32_t m = 0;
  for (uint8_t i = 0; i < kCount; i++)
    if (kHats[i].unlocked(s, level)) m |= 1u << i;
  return m;
}

}  // namespace hats
