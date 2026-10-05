#include "Widgets.h"
#include <math.h>
#include <stdio.h>

using namespace theme;

namespace ui {

void icon(gfx::Surface& s, Glyph g, int16_t cx, int16_t cy, int16_t size, uint16_t c, uint8_t a) {
  float u = size / 16.0f;  // icons are designed on a 16x16 grid
  auto X = [&](float v) { return (int16_t)lroundf(cx + (v - 8) * u); };
  auto Y = [&](float v) { return (int16_t)lroundf(cy + (v - 8) * u); };
  auto R = [&](float v) { return v * u; };
  int16_t t = size >= 20 ? 2 : 1;
  switch (g) {
    case Glyph::Home:
      s.fillTriangle(X(1), Y(8), X(8), Y(1.5f), X(15), Y(8), c, a);
      s.fillRect(X(3.5f), Y(8), X(12.5f) - X(3.5f), Y(15) - Y(8), c, a);
      s.fillRect(X(6.8f), Y(10.5f), X(9.2f) - X(6.8f), Y(15) - Y(10.5f), 0x0000, a);
      break;
    case Glyph::Stats:
      s.fillRect(X(2), Y(9), R(3), Y(15) - Y(9), c, a);
      s.fillRect(X(6.5f), Y(4), R(3), Y(15) - Y(4), c, a);
      s.fillRect(X(11), Y(1), R(3), Y(15) - Y(1), c, a);
      break;
    case Glyph::Trophy:
      s.fillRoundRect(X(4), Y(1.5f), X(12) - X(4), Y(8) - Y(1.5f), (int16_t)R(3), c, a);
      s.ring(X(4), Y(4.5f), R(2.8f), R(1.2f), c, a, 180, 180);
      s.ring(X(12), Y(4.5f), R(2.8f), R(1.2f), c, a, 0, 180);
      s.fillRect(X(7), Y(8), R(2), Y(12) - Y(8), c, a);
      s.fillRect(X(4.5f), Y(12), X(11.5f) - X(4.5f), Y(14.5f) - Y(12), c, a);
      break;
    case Glyph::Gear:
      for (int i = 0; i < 8; i++) {
        float ang = i * 3.14159f / 4;
        s.fillCircle(cx + sinf(ang) * R(5.6f), cy - cosf(ang) * R(5.6f), R(1.7f), c, a);
      }
      s.ring(cx, cy, R(5.6f), R(2.6f), c, a);
      break;
    case Glyph::Wifi:
      for (int i = 0; i < 3; i++) s.ring(X(8), Y(13), R(4 + i * 3.6f), R(1.6f), c, a, 315, 90);
      s.fillCircle(X(8), Y(13), R(1.6f), c, a);
      break;
    case Glyph::Ble:
      s.line(X(5), Y(4.5f), X(11), Y(10.5f), c, a);
      s.line(X(5), Y(11.5f), X(11), Y(5.5f), c, a);
      s.line(X(8), Y(1.5f), X(8), Y(14.5f), c, a);
      s.line(X(8), Y(1.5f), X(11), Y(5.5f), c, a);
      s.line(X(8), Y(14.5f), X(11), Y(10.5f), c, a);
      if (t > 1) {
        s.line(X(8) + 1, Y(1.5f), X(8) + 1, Y(14.5f), c, a);
        s.line(X(5), Y(4.5f) + 1, X(11), Y(10.5f) + 1, c, a);
        s.line(X(5), Y(11.5f) - 1, X(11), Y(5.5f) - 1, c, a);
      }
      break;
    case Glyph::Mesh:
      s.line(X(3), Y(12), X(8), Y(3), c, a);
      s.line(X(8), Y(3), X(13), Y(12), c, a);
      s.line(X(3), Y(12), X(13), Y(12), c, a);
      s.line(X(8), Y(3), X(8), Y(9), c, a);
      s.fillCircle(X(3), Y(12), R(2.2f), c, a);
      s.fillCircle(X(13), Y(12), R(2.2f), c, a);
      s.fillCircle(X(8), Y(3), R(2.2f), c, a);
      s.fillCircle(X(8), Y(9), R(1.8f), c, a);
      break;
    case Glyph::Goblin:
      s.fillTriangle(X(1), Y(4), X(6), Y(7), X(5), Y(10), c, a);
      s.fillTriangle(X(15), Y(4), X(10), Y(7), X(11), Y(10), c, a);
      s.fillCircle(X(8), Y(9), R(5), c, a);
      s.fillCircle(X(6.2f), Y(8.5f), R(1.1f), theme::kRed, a);
      s.fillCircle(X(9.8f), Y(8.5f), R(1.1f), theme::kRed, a);
      break;
    case Glyph::Star: {
      int16_t px[10], py[10];
      for (int i = 0; i < 10; i++) {
        float ang = i * 3.14159f / 5, rr = (i & 1) ? R(3) : R(7);
        px[i] = (int16_t)lroundf(cx + sinf(ang) * rr);
        py[i] = (int16_t)lroundf(cy + 0.5f * u - cosf(ang) * rr);
      }
      for (int i = 0; i < 10; i += 2) {
        s.fillTriangle(cx, cy, px[i], py[i], px[(i + 1) % 10], py[(i + 1) % 10], c, a);
        s.fillTriangle(cx, cy, px[i], py[i], px[(i + 9) % 10], py[(i + 9) % 10], c, a);
      }
      break;
    }
    case Glyph::Map:
      s.fillCircle(X(8), Y(6), R(4.5f), c, a);
      s.fillTriangle(X(4.2f), Y(7.5f), X(11.8f), Y(7.5f), X(8), Y(15), c, a);
      s.fillCircle(X(8), Y(6), R(1.8f), 0x0000, a);
      break;
    case Glyph::Clock:
      s.ring(X(8), Y(8), R(7), R(1.6f), c, a);
      s.line(X(8), Y(8), X(8), Y(3.5f), c, a);
      s.line(X(8), Y(8), X(11), Y(9.5f), c, a);
      break;
    case Glyph::Paw:
      s.fillCircle(X(8), Y(11), R(3.6f), c, a);
      s.fillCircle(X(3.5f), Y(7), R(1.8f), c, a);
      s.fillCircle(X(6.5f), Y(4), R(1.8f), c, a);
      s.fillCircle(X(9.5f), Y(4), R(1.8f), c, a);
      s.fillCircle(X(12.5f), Y(7), R(1.8f), c, a);
      break;
    case Glyph::Lock:
      s.ring(X(8), Y(6.5f), R(4), R(1.6f), c, a, 270, 180);
      s.fillRect(X(4) - 1, Y(6.5f), (int16_t)R(1.6f), Y(8) - Y(6.5f) + 1, c, a);
      s.fillRect(X(12) - (int16_t)R(1.6f) + 1, Y(6.5f), (int16_t)R(1.6f), Y(8) - Y(6.5f) + 1, c, a);
      s.fillRoundRect(X(2.5f), Y(7.5f), X(13.5f) - X(2.5f), Y(15) - Y(7.5f), (int16_t)R(1.5f), c, a);
      break;
    case Glyph::Signal:
      for (int i = 0; i < 4; i++) s.fillRect(X(1.5f + i * 3.6f), Y(12 - i * 3), R(2.4f), Y(15) - Y(12 - i * 3), c, a);
      break;
    case Glyph::Sd:
      s.fillRoundRect(X(3), Y(1), X(13) - X(3), Y(15) - Y(1), (int16_t)R(1.5f), c, a);
      s.fillTriangle(X(9.5f), Y(1), X(13.2f), Y(1), X(13.2f), Y(4.5f), 0x0000, a);
      for (int i = 0; i < 3; i++) s.fillRect(X(5 + i * 2.2f), Y(2.5f), R(1.2f), R(3), 0x0000, a);
      break;
    case Glyph::Gps:
      s.ring(X(8), Y(8), R(5), R(1.5f), c, a);
      s.fillCircle(X(8), Y(8), R(1.8f), c, a);
      s.fillRect(X(7.3f), Y(0.5f), R(1.4f), R(3), c, a);
      s.fillRect(X(7.3f), Y(12.5f), R(1.4f), R(3), c, a);
      s.fillRect(X(0.5f), Y(7.3f), R(3), R(1.4f), c, a);
      s.fillRect(X(12.5f), Y(7.3f), R(3), R(1.4f), c, a);
      break;
    case Glyph::Beacon:
      s.fillCircle(X(8), Y(8), R(2), c, a);
      s.ring(X(8), Y(8), R(5), R(1.4f), c, a, 300, 120);
      s.ring(X(8), Y(8), R(5), R(1.4f), c, a, 120, 120);
      s.ring(X(8), Y(8), R(7.8f), R(1.4f), c, a, 305, 110);
      s.ring(X(8), Y(8), R(7.8f), R(1.4f), c, a, 125, 110);
      break;
    case Glyph::Heart:
      s.fillCircle(X(5), Y(6), R(3.6f), c, a);
      s.fillCircle(X(11), Y(6), R(3.6f), c, a);
      s.fillTriangle(X(1.5f), Y(7.2f), X(14.5f), Y(7.2f), X(8), Y(14.5f), c, a);
      break;
    case Glyph::Check:
      for (int k = 0; k < t + 1; k++) {
        s.line(X(2), Y(8) + k, X(6), Y(12) + k, c, a);
        s.line(X(6), Y(12) + k, X(14), Y(3) + k, c, a);
      }
      break;
    case Glyph::Chevron:
      for (int k = 0; k < t + 1; k++) {
        s.line(X(5) + k, Y(2), X(11) + k, Y(8), c, a);
        s.line(X(11) + k, Y(8), X(5) + k, Y(14), c, a);
      }
      break;
  }
}

Glyph iconFor(Icon i) {
  switch (i) {
    case Icon::Wifi: return Glyph::Wifi;
    case Icon::Ble: return Glyph::Ble;
    case Icon::Mesh: return Glyph::Mesh;
    case Icon::Goblin: return Glyph::Goblin;
    case Icon::Star: return Glyph::Star;
    case Icon::Map: return Glyph::Map;
    case Icon::Clock: return Glyph::Clock;
    case Icon::Paw: return Glyph::Paw;
    case Icon::Trophy: return Glyph::Trophy;
    case Icon::Lock: return Glyph::Lock;
    case Icon::Signal: return Glyph::Signal;
  }
  return Glyph::Star;
}

void panel(gfx::Surface& s, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t edge, uint8_t a) {
  s.fillRoundRect(x, y, w, h, 7, kPanel, a);
  s.gradientV(x + 3, y + 1, w - 6, 8, kPanelHi, kPanel, (uint8_t)(a / 2));
  s.roundRect(x, y, w, h, 7, edge, 180);
}

void toggle(gfx::Surface& s, int16_t x, int16_t y, float on, uint16_t c) {
  uint16_t track = gfx::mix(kFaint, gfx::dim(c, 140), (uint8_t)(on * 255));
  s.fillRoundRect(x, y, 34, 18, 9, track);
  float kx = x + 9 + on * 16;
  if (on > 0.5f) s.glow(kx, y + 9, 14, c, (uint8_t)(90 * on));
  s.fillCircle(kx, y + 9, 7, gfx::mix(kDim, kText, (uint8_t)(on * 255)));
}

void progressRing(gfx::Surface& s, int16_t cx, int16_t cy, float r, float thick, float p, uint32_t now) {
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  s.ring(cx, cy, r, thick, kFaint, 160);
  float sweep = p * 360;
  s.ringGradient(cx, cy, r, thick, kCyan, kGreen, sweep);
  float ang = sweep * 3.14159f / 180;
  float hx = cx + sinf(ang) * (r - thick / 2), hy = cy - cosf(ang) * (r - thick / 2);
  float pulse = 0.5f + 0.5f * sinf(now / 300.0f);
  s.glow(hx, hy, thick * 2.6f, kGreen, (uint8_t)(120 + 80 * pulse));
  s.fillCircle(hx, hy, thick * 0.55f, kText);
}

void bar(gfx::Surface& s, int16_t x, int16_t y, int16_t w, int16_t h, float p, uint16_t c) {
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  s.fillRoundRect(x, y, w, h, h / 2, kFaint, 160);
  int16_t fw = (int16_t)(w * p);
  if (fw >= h) {
    s.fillRoundRect(x, y, fw, h, h / 2, c);
    s.hline(x + h / 2, y + 1, fw - h, 0xFFFF, 60);
  }
}

uint16_t tierColor(Tier t, uint32_t now) {
  switch (t) {
    case kBronze: return kBronzeC;
    case kSilver: return kSilverC;
    case kGold: return kGoldC;
    default: return gfx::hue((uint8_t)(now / 12), 170, 255);  // legendary: shifting rainbow
  }
}

const char* tierName(Tier t) {
  switch (t) {
    case kBronze: return "BRONZE";
    case kSilver: return "SILVER";
    case kGold: return "GOLD";
    default: return "LEGENDARY";
  }
}

void medallion(gfx::Surface& s, int16_t cx, int16_t cy, float r, const AchievementDef& a, bool unlocked,
               uint32_t now) {
  if (!unlocked) {
    s.fillCircle(cx, cy, r, gfx::hex(0x0A161B));
    s.ring(cx, cy, r, 1.5f, kFaint);
    icon(s, a.secret ? Glyph::Lock : iconFor(a.icon), cx, cy, (int16_t)(r * 1.0f), kFaint);
    return;
  }
  uint16_t tc = tierColor(a.tier, now);
  if (a.tier >= kGold) s.glow(cx, cy, r * 1.7f, tc, 70);
  s.fillCircle(cx, cy, r, gfx::dim(tc, 70));
  s.fillCircle(cx, cy - r * 0.15f, r * 0.8f, gfx::dim(tc, 100), 120);
  s.ring(cx, cy, r, r * 0.2f, tc);
  s.ring(cx, cy, r * 0.8f, 1, gfx::mix(tc, 0xFFFF, 120), 120);
  icon(s, iconFor(a.icon), cx, cy, (int16_t)(r * 1.05f), kText);
  // Specular glint sweeping across.
  float ph = fmodf(now / 2500.0f + cx * 0.013f, 1.0f);
  if (ph < 0.25f) s.glow(cx - r + ph * 8 * r, cy - r * 0.4f, r * 0.5f, 0xFFFF, (uint8_t)(110 * sinf(ph * 4 * 3.14159f)));
}

void formatCount(char* out, size_t n, uint32_t v) {
  if (v >= 1000000) snprintf(out, n, "%lu.%luM", (unsigned long)(v / 1000000), (unsigned long)(v / 100000 % 10));
  else if (v >= 10000) snprintf(out, n, "%luk", (unsigned long)(v / 1000));
  else if (v >= 1000) snprintf(out, n, "%lu,%03lu", (unsigned long)(v / 1000), (unsigned long)(v % 1000));
  else snprintf(out, n, "%lu", (unsigned long)v);
}

}  // namespace ui
