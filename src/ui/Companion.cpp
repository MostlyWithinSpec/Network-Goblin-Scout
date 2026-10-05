#include "Companion.h"
#include <math.h>
#include <initializer_list>
#include "HatArt.h"
#include "Theme.h"
#include "assets/GoblinArt.h"

using namespace theme;

namespace {
const float kPi = 3.14159265f;
const uint16_t kLid = gfx::hex(0x4C8A2A);
const uint16_t kLidLine = gfx::hex(0x1B3510);

float wave(uint32_t now, float periodS, float phase = 0) { return sinf((now / 1000.0f / periodS + phase) * 2 * kPi); }

// Cheap deterministic noise for flicker (0..255).
uint8_t noise(uint32_t v) {
  v ^= v >> 13; v *= 0x5bd1e995; v ^= v >> 15;
  return (uint8_t)v;
}

bool hopping(CState st) {
  return st == CState::Discovered || st == CState::Excited || st == CState::LevelUp || st == CState::Achievement;
}

void shadow(gfx::Surface& s, int16_t cx, int16_t y, float rx, uint8_t a) {
  for (int dy = -3; dy <= 3; dy++) {
    float k = sqrtf(1 - (dy * dy) / 12.0f);
    int16_t w = (int16_t)(rx * k);
    s.hline(cx - w, y + dy, 2 * w, 0x0000, (uint8_t)(a * k));
  }
}
}  // namespace

void Companion::react(CState s, uint32_t ms, uint32_t now) {
  temp_ = s;
  tempUntil_ = now + ms;
}

CState Companion::state(uint32_t now) const {
  if (tempUntil_ && (int32_t)(now - tempUntil_) < 0) return temp_;
  return base_;
}

void Companion::draw(gfx::Surface& s, int16_t x, int16_t y, uint32_t now, const Frame* custom) const {
  CState st = state(now);
  if (custom && custom->px) {  // SD sprite pack
    int scale = (custom->w <= 64 && custom->h <= 64) ? 2 : 1;
    float bob = hopping(st) ? -fabsf(wave(now, 0.5f)) * 8 : wave(now, 2.4f) * 2;
    shadow(s, x, y + 2, custom->w * scale * 0.35f, 90);
    s.blitKeyed(custom->px, custom->w, custom->h, x - custom->w * scale / 2,
                (int16_t)(y - custom->h * scale + bob), 0xF81F, scale);
    return;
  }
  Look look;
  look.hat = hat_;
  if (droopy_ && st == CState::Idle) look.bright = 200;
  drawGoblin(s, x, y, now, st, look);
}

void Companion::drawGoblin(gfx::Surface& s, int16_t x, int16_t y, uint32_t now, CState st, const Look& look) {
  using namespace art;
  bool sleeping = st == CState::Sleeping;
  bool scanning = st == CState::Scanning || st == CState::Searching;
  bool hop = hopping(st);

  // Motion: breathing bob, a quicker jitter while scanning, hops when excited.
  float bob = 0, sx = 1, sy = 1;
  if (hop) {
    float h = fabsf(wave(now, 0.55f));
    bob = -h * 12;
    float land = 1 - h;  // squash near the ground, stretch in the air
    sy = 1 - 0.07f * land * land + 0.03f * h;
    sx = 1 + 0.05f * land * land;
  } else if (sleeping) {
    sy = 0.985f + 0.015f * wave(now, 3.2f);
  } else if (scanning) {
    bob = wave(now, 0.9f) * 1.5f;
  } else {
    bob = wave(now, 2.4f) * 2.5f;
    sy = 1 + 0.012f * wave(now, 2.4f, 0.25f);
  }
  float scX = look.scale * sx, scY = look.scale * sy;
  int16_t dw = (int16_t)lroundf(kGoblinW * scX), dh = (int16_t)lroundf(kGoblinH * scY);
  int16_t left = x - dw / 2;
  int16_t top = (int16_t)(y - dh + bob);

  // Shadow shrinks as the goblin leaves the ground.
  shadow(s, x, y, dw * 0.33f * (1 + bob / 40.0f), (uint8_t)(110 + bob * 3));

  // Aura colour by mood.
  uint16_t auraC = kCyan;
  float auraA = 45 + 20 * wave(now, 2.0f);
  if (look.tint) { auraC = look.tintColor; auraA = 80 + 30 * wave(now, 1.0f); }
  else if (st == CState::LevelUp || st == CState::Achievement) { auraC = kGoldC; auraA = 130 + 50 * wave(now, 0.4f); }
  else if (hop) { auraC = kGreen; auraA = 90 + 40 * wave(now, 0.5f); }
  else if (scanning) { auraA = 60 + 30 * wave(now, 0.7f); }
  else if (sleeping) { auraC = kViolet; auraA = 25; }
  if (look.aura) {
    if (look.scale == 1.0f)
      s.alphaMask(kGoblinAura, kGoblinAuraW, kGoblinAuraH, left - kGoblinAuraPad, top - kGoblinAuraPad + (dh - kGoblinH),
                  auraC, (uint8_t)(auraA * look.alpha / 255));
    else
      s.glow(x, top + dh * 0.5f, dw * 0.62f, auraC, (uint8_t)(auraA * look.alpha / 255));
  }

  // Pixel effects: glowing eyes, flickering device screen.
  gfx::SpriteFx fx;
  fx.scaleX = scX;
  fx.scaleY = scY;
  fx.flipX = look.flip;
  fx.alpha = look.alpha;
  fx.skinTintOn = look.tint;
  fx.skinTint = look.tintColor;
  fx.eyeColor = (st == CState::LevelUp || st == CState::Achievement) ? kGoldC : kRed;
  fx.eyeMix = (uint8_t)(70 + 60 * (0.5f + 0.5f * wave(now, 1.7f)));
  if (scanning) {
    uint8_t n = noise(now / 70);
    fx.screenColor = n > 200 ? 0xFFFF : kCyan;
    fx.screenMix = (uint8_t)(110 + (n >> 2));
  } else if (hop) {
    fx.screenColor = kGreen;
    fx.screenMix = 180;
  } else {
    fx.screenColor = kCyan;
    fx.screenMix = (uint8_t)(50 + 40 * (0.5f + 0.5f * wave(now, 2.0f)));
  }
  if (sleeping) fx.bright = 150;
  else if (look.bright != 255) fx.bright = look.bright;
  s.sprite(kGoblinPx, kGoblinAlpha, kGoblinClass, kGoblinW, kGoblinH, left, top, fx);

  // Blink every ~3.7 s (always shut when asleep): skin-coloured lids over both eyes.
  bool blink = sleeping || (now % 3700) < 130 || ((now + 400) % 9100) < 110;
  if (blink) {
    for (const int16_t* e : {kGoblinEyeL, kGoblinEyeR}) {
      int16_t ex0 = e[0], ex1 = e[2];
      if (look.flip) { ex0 = kGoblinW - 1 - e[2]; ex1 = kGoblinW - 1 - e[0]; }
      int16_t x0 = left + (int16_t)(ex0 * scX) - 1, x1 = left + (int16_t)((ex1 + 1) * scX) + 1;
      int16_t y0 = top + (int16_t)(e[1] * scY) - 1, y1 = top + (int16_t)((e[3] + 1) * scY) + 1;
      uint16_t lid = look.tint ? gfx::dim(look.tintColor, 170) : kLid;
      if (sleeping) lid = gfx::dim(lid, 150);
      s.fillRoundRect(x0, y0, x1 - x0, y1 - y0, 2, lid, look.alpha);
      s.hline(x0 + 1, y1 - 2, x1 - x0 - 2, kLidLine, look.alpha);
    }
  }

  // Hat: anchored on the crown of the head (sprite x 48, y 17).
  if (look.hat) {
    float hx = left + (look.flip ? kGoblinW - 1 - 48 : 48) * scX;
    ui::drawHat(s, look.hat, hx, top + 17 * scY, scX, now, look.flip);
  }

  // Device screen position (for signal arcs).
  int16_t scrX = left + (int16_t)((look.flip ? kGoblinW - 1 - kGoblinScreenX : kGoblinScreenX) * scX);
  int16_t scrY = top + (int16_t)(kGoblinScreenY * scY);
  if (scanning) {
    float base = look.flip ? 270 - 50 : 30;
    for (int i = 0; i < 3; i++) {
      float ph = fmodf(now / 900.0f + i / 3.0f, 1.0f);
      s.ring(scrX, scrY, 8 + ph * 26, 2, kCyan, (uint8_t)(200 * (1 - ph) * look.alpha / 255), base, 100);
    }
  }
  if (sleeping) {
    for (int i = 0; i < 3; i++) {
      float ph = fmodf(now / 2600.0f + i / 3.0f, 1.0f);
      int16_t zx = left + dw * 0.62f + ph * 22 + 4 * sinf(ph * 6);
      int16_t zy = top + 6 - ph * 34;
      const gfx::Font& f = i == 1 ? fTitle() : fSmall();
      s.text(f, zx, zy, "z", kText, (uint8_t)(220 * sinf(ph * kPi)));
    }
  }
}

void Companion::drawSmall(gfx::Surface& s, int16_t x, int16_t y, uint32_t now, const Look& look) {
  using namespace art;
  float bob = wave(now, 0.8f) * 2;
  float scX = look.scale, scY = look.scale;
  int16_t dw = (int16_t)(kGoblinSmallW * scX), dh = (int16_t)(kGoblinSmallH * scY);
  shadow(s, x, y, dw * 0.33f, 90);
  if (look.aura) s.glow(x, y - dh * 0.5f, dw * 0.7f, look.tint ? look.tintColor : kCyan, 90);
  gfx::SpriteFx fx;
  fx.scaleX = scX;
  fx.scaleY = scY;
  fx.flipX = look.flip;
  fx.alpha = look.alpha;
  fx.skinTintOn = look.tint;
  fx.skinTint = look.tintColor;
  int16_t left = x - dw / 2, top = (int16_t)(y - dh + bob);
  s.sprite(kGoblinSmallPx, kGoblinSmallAlpha, kGoblinSmallClass, kGoblinSmallW, kGoblinSmallH, left, top, fx);
  if (look.hat) {  // same anchor as the big goblin, scaled to the small sprite
    const float k = (float)kGoblinSmallW / kGoblinW;
    float hx = left + (look.flip ? kGoblinW - 1 - 48 : 48) * k * scX;
    ui::drawHat(s, look.hat, hx, top + 17 * k * scY, k * scX, now, look.flip);
  }
}
