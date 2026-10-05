#pragma once
#include "../core/Achievements.h"
#include "Theme.h"

namespace ui {
enum class Glyph : uint8_t {
  Home, Stats, Trophy, Gear, Wifi, Ble, Mesh, Goblin, Star, Map, Clock, Paw, Lock, Signal, Sd, Gps, Beacon,
  Heart, Check, Chevron
};

// Vector icon centred on (cx, cy), roughly `size` px across.
void icon(gfx::Surface& s, Glyph g, int16_t cx, int16_t cy, int16_t size, uint16_t c, uint8_t a = 255);
Glyph iconFor(Icon i);

// Frosted panel with a thin glowing edge.
void panel(gfx::Surface& s, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t edge = theme::kEdge,
           uint8_t a = 210);
void toggle(gfx::Surface& s, int16_t x, int16_t y, float on01, uint16_t c = theme::kCyan);  // 34x18
void progressRing(gfx::Surface& s, int16_t cx, int16_t cy, float r, float thick, float p, uint32_t now);
void bar(gfx::Surface& s, int16_t x, int16_t y, int16_t w, int16_t h, float p, uint16_t c);
uint16_t tierColor(Tier t, uint32_t now);
const char* tierName(Tier t);
void medallion(gfx::Surface& s, int16_t cx, int16_t cy, float r, const AchievementDef& a, bool unlocked,
               uint32_t now);
void formatCount(char* out, size_t n, uint32_t v);  // 1234 -> "1,234", 1234567 -> "1.2M"
}  // namespace ui
