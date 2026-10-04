#pragma once
#include "Types.h"

struct AchievementDef {
  const char* id;     // stable key — saved to SD and (later) synced. Never reorder/rename.
  const char* name;
  const char* desc;
  bool (*check)(const Stats&);
};

extern const AchievementDef ACHIEVEMENTS[];
extern const size_t ACHIEVEMENT_COUNT;

namespace progression {
uint16_t levelForXp(uint32_t xp);
uint32_t xpForLevel(uint16_t level);
const char* title(uint16_t level);
}  // namespace progression
