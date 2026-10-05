#pragma once
#include <stdint.h>
#include "Types.h"

// Cosmetic hats, unlocked by milestones. Pure logic; the UI draws them (ui/Hats.cpp).
// The equipped hat id is saved in settings and sent in the goblin beacon, so ids are
// append-only: never reorder or reuse. 0 = no hat.
namespace hats {
struct Def {
  const char* name;
  const char* how;   // unlock condition, shown in the wardrobe
  bool (*unlocked)(const Stats& s, uint16_t level);
};

extern const Def kHats[];   // index = hat id - 1
extern const uint8_t kCount;

bool unlocked(uint8_t id, const Stats& s, uint16_t level);  // id 0 is always "unlocked"
uint32_t unlockedMask(const Stats& s, uint16_t level);      // bit (id - 1)
uint32_t seasonBit(uint8_t month);  // Stats::seasonMask bit earned in this month (0 = none)
}  // namespace hats
