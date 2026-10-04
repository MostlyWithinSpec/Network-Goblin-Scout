#pragma once
#include <Arduino_GFX_Library.h>
#include "Sprites.h"

// The digital pet. Has a "base" mood (idle/scanning/sleeping) plus short
// reaction states (discovered, level up...) that expire back to the base.
class Companion {
 public:
  SpritePack pack;
  void setBase(CState s) { base_ = s; }
  void react(CState s, uint32_t ms);
  CState state() const;
  void draw(Arduino_GFX* g, int16_t x, int16_t y, uint32_t tick);

 private:
  CState base_ = CState::Idle;
  CState temp_ = CState::Idle;
  uint32_t tempUntil_ = 0;
  void drawGoblin(Arduino_GFX* g, int16_t x, int16_t y, CState s, uint32_t tick);
};
