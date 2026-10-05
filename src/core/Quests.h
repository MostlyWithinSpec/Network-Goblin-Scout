#pragma once
#include <stddef.h>
#include "Types.h"

// Daily-ish quests: a board of three "grow this counter by N" challenges. Pure logic
// (no Arduino), so it's unit-tested on the PC (test/test_quests.cpp).
namespace quests {

// Saved on SD by number: append only, never reorder.
enum Type : uint8_t {
  kNewWifi, kNew5G, kNewBle, kNewMesh, kNewChannel, kMeetGoblin, kPetGoblin, kScans,
  kHidden, kWpa3, kNewArea, kWifi6, kEnterprise, kOpen, kTypeCount
};

uint32_t metric(const Stats& s, uint8_t type);           // current value of the counter
uint32_t progress(const Stats& s, const Quest& q);       // capped at target
uint32_t reward(const Quest& q);                         // XP for finishing it
void describe(const Quest& q, char* out, size_t n);      // "Sniff out 20 new networks"
// Deal a fresh board. Only offers quests this goblin can plausibly do (no mesh quests
// before it has ever heard a mesh device, no GPS quests without GPS, ...).
void deal(const Stats& s, uint16_t level, uint32_t seed, QuestBoard& board);

}  // namespace quests
