#include "Companion.h"
#include "config.h"

namespace {
const uint16_t SKIN = 0x5E8A;     // goblin green
const uint16_t SKIN_DK = 0x3C66;
const uint16_t EYE = 0xFFFF;
const uint16_t PUPIL = 0x0000;
const uint16_t ACCENT = 0xFD20;   // Network Goblin orange
}  // namespace

void Companion::react(CState s, uint32_t ms) {
  temp_ = s;
  tempUntil_ = millis() + ms;
}

CState Companion::state() const {
  if (tempUntil_ && (int32_t)(millis() - tempUntil_) < 0) return temp_;
  return base_;
}

void Companion::draw(Arduino_GFX* g, int16_t x, int16_t y, uint32_t tick) {
  CState s = state();
  const Frame* f = pack.frame(s, tick / 3);  // ~3 fps animation at 10 fps UI
  if (!f) { drawGoblin(g, x, y, s, tick); return; }

  // Pixel-art packs: scale small sprites up to fill the box.
  int scale = (f->w <= SPRITE_BOX / 2 && f->h <= SPRITE_BOX / 2) ? 2 : 1;
  int16_t ox = x + (SPRITE_BOX - f->w * scale) / 2;
  int16_t oy = y + (SPRITE_BOX - f->h * scale) / 2;
  if (scale == 1) {
    g->draw16bitRGBBitmapWithTranColor(ox, oy, f->px, SPRITE_TRANSPARENT, f->w, f->h);
    return;
  }
  for (uint16_t j = 0; j < f->h; j++)
    for (uint16_t i = 0; i < f->w; i++) {
      uint16_t c = f->px[j * f->w + i];
      if (c != SPRITE_TRANSPARENT) g->fillRect(ox + i * 2, oy + j * 2, 2, 2, c);
    }
}

// Built-in placeholder goblin, drawn with primitives so the device works with no SD card.
void Companion::drawGoblin(Arduino_GFX* g, int16_t x, int16_t y, CState s, uint32_t tick) {
  int16_t cx = x + SPRITE_BOX / 2;
  int16_t cy = y + SPRITE_BOX / 2 + 10;
  bool bounce = (s == CState::Discovered || s == CState::Excited || s == CState::LevelUp ||
                 s == CState::Achievement);
  if (bounce) cy -= (tick % 4 < 2) ? 6 : 0;
  bool sleeping = (s == CState::Sleeping);

  // antenna + signal arcs
  g->drawLine(cx, cy - 38, cx + 10, cy - 58, SKIN_DK);
  g->fillCircle(cx + 10, cy - 58, 4, ACCENT);
  if (s == CState::Scanning || s == CState::Searching) {
    int arcs = tick % 4;
    for (int a = 1; a <= arcs; a++) g->drawCircle(cx + 10, cy - 58, 4 + a * 5, ACCENT);
  }

  // ears
  g->fillTriangle(cx - 34, cy - 12, cx - 62, cy - 34, cx - 26, cy + 6, SKIN);
  g->fillTriangle(cx + 34, cy - 12, cx + 62, cy - 34, cx + 26, cy + 6, SKIN);
  // head
  g->fillCircle(cx, cy, 40, SKIN);
  g->drawCircle(cx, cy, 40, SKIN_DK);

  // eyes
  bool blink = (tick % 40) == 0;
  if (sleeping || blink) {
    g->drawFastHLine(cx - 22, cy - 6, 14, PUPIL);
    g->drawFastHLine(cx + 8, cy - 6, 14, PUPIL);
  } else {
    int look = 0;
    if (s == CState::Scanning || s == CState::Searching) look = ((tick / 5) % 3) - 1;  // eyes dart around
    g->fillCircle(cx - 15, cy - 6, 9, EYE);
    g->fillCircle(cx + 15, cy - 6, 9, EYE);
    g->fillCircle(cx - 15 + look * 4, cy - 5, 4, PUPIL);
    g->fillCircle(cx + 15 + look * 4, cy - 5, 4, PUPIL);
  }

  // mouth
  switch (s) {
    case CState::Discovered:
    case CState::Excited:
    case CState::LevelUp:
    case CState::Achievement:
      g->fillCircle(cx, cy + 16, 10, PUPIL);
      g->fillRect(cx - 11, cy + 5, 22, 11, SKIN);
      g->fillTriangle(cx - 6, cy + 16, cx - 2, cy + 16, cx - 4, cy + 21, EYE);  // fang
      break;
    case CState::LowBattery:
    case CState::Offline:
      g->drawLine(cx - 8, cy + 20, cx + 8, cy + 16, PUPIL);
      break;
    default:
      g->drawLine(cx - 10, cy + 16, cx + 10, cy + 16, PUPIL);
      g->drawPixel(cx - 11, cy + 15, PUPIL);
      g->drawPixel(cx + 11, cy + 15, PUPIL);
  }

  if (sleeping) {
    g->setTextColor(0xFFFF);
    g->setTextSize(2);
    g->setCursor(cx + 36, cy - 44 - (tick / 5) % 6);
    g->print("z");
  }
  if (s == CState::LevelUp || s == CState::Achievement) {
    for (int i = 0; i < 6; i++) {  // sparkles
      int16_t sx = x + ((tick * 7 + i * 37) % SPRITE_BOX);
      int16_t sy = y + ((tick * 3 + i * 53) % 40);
      g->drawFastHLine(sx - 3, sy, 7, 0xFFE0);
      g->drawFastVLine(sx, sy - 3, 7, 0xFFE0);
    }
  }
}
