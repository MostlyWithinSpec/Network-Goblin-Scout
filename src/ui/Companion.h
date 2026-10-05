#pragma once
#include <stdint.h>
#include "gfx/Surface.h"

enum class CState : uint8_t {
  Idle, Scanning, Searching, Discovered, Excited, LevelUp, Achievement,
  Uploading, Sleeping, LowBattery, Offline, SyncDone, COUNT
};

// One frame of an SD-card sprite pack (RGB565, magenta = transparent).
struct Frame {
  uint16_t* px = nullptr;
  uint16_t w = 0, h = 0;
};

// The digital pet. Has a "base" mood (idle/scanning/sleeping) plus short reaction
// states (discovered, level up...) that expire back to the base. The built-in goblin
// is the Network Goblin logo, animated: breathing bob, hops, blinks, glowing eyes and
// a device screen that flickers while scanning.
class Companion {
 public:
  void setBase(CState s) { base_ = s; }
  void react(CState s, uint32_t ms, uint32_t now);
  CState state(uint32_t now) const;

  struct Look {
    float scale = 1;
    bool flip = false;
    bool tint = false;      // recolour skin (other goblins)
    uint16_t tintColor = 0;
    uint8_t alpha = 255;
    bool aura = true;
  };
  // (x, y) = bottom-centre (feet) of the goblin.
  void draw(gfx::Surface& s, int16_t x, int16_t y, uint32_t now, const Frame* custom = nullptr) const;
  static void drawGoblin(gfx::Surface& s, int16_t x, int16_t y, uint32_t now, CState st, const Look& look);
  static void drawSmall(gfx::Surface& s, int16_t x, int16_t y, uint32_t now, const Look& look);

 private:
  CState base_ = CState::Idle;
  CState temp_ = CState::Idle;
  uint32_t tempUntil_ = 0;
};
