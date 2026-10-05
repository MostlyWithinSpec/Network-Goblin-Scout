#pragma once
#include "Types.h"

enum Tier : uint8_t { kBronze = 0, kSilver = 1, kGold = 2, kLegendary = 3 };
enum class Icon : uint8_t { Wifi, Ble, Mesh, Goblin, Star, Map, Clock, Paw, Trophy, Lock, Signal };

struct AchievementDef {
  const char* id;     // stable key — saved to SD and (later) synced. Never reorder/rename.
  const char* name;
  const char* desc;
  Tier tier;
  Icon icon;
  bool secret;        // description hidden until unlocked
  bool (*check)(const Stats&);
};

extern const AchievementDef ACHIEVEMENTS[];
extern const size_t ACHIEVEMENT_COUNT;

namespace progression {
uint16_t levelForXp(uint32_t xp);
uint32_t xpForLevel(uint16_t level);
const char* title(uint16_t level);
}  // namespace progression
