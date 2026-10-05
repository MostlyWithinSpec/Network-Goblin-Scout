#pragma once
#include <stdint.h>

// Goblin sniff-offs: when two goblins meet they sniff each other's hoards and the bigger
// hoard wins. Each side only knows the other's beacon, so the hoard travels as a 3-bit
// "tier" in beacon flags bits 5-7 (older firmware sends 0 there). Both goblins compute the
// same verdict from the same numbers. Pure (no Arduino): tested in test/test_peer.cpp.
namespace sniff {

// Hoard = unique Wi-Fi + Bluetooth + mesh devices. Tier upper bounds, roughly x2.5 each.
inline uint8_t tier(uint32_t hoard) {
  static const uint32_t kBounds[] = {50, 150, 400, 1000, 2500, 6000, 15000};
  uint8_t t = 0;
  while (t < 7 && hoard >= kBounds[t]) t++;
  return t;
}

inline const char* tierName(uint8_t t) {
  static const char* const kNames[] = {"Pocket lint", "Small stash", "Modest pile", "Respectable hoard",
                                       "Big hoard",   "Huge hoard",  "Dragon hoard", "Legendary heap"};
  return kNames[t & 7];
}

enum Result : uint8_t { kLose = 0, kDraw = 1, kWin = 2 };

// Bigger hoard tier wins; the higher level breaks a tie.
inline Result judge(uint8_t myTier, uint16_t myLevel, uint8_t theirTier, uint16_t theirLevel) {
  if (myTier != theirTier) return myTier > theirTier ? kWin : kLose;
  if (myLevel != theirLevel) return myLevel > theirLevel ? kWin : kLose;
  return kDraw;
}

}  // namespace sniff
