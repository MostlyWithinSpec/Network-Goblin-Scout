#include "HatArt.h"
#include <math.h>
#include "Theme.h"

using gfx::hex;

namespace ui {
namespace {
// A few floats per hat per frame is fine (the per-pixel work is integer, in Surface).
struct Pen {
  gfx::Surface& s;
  float cx, cy, sc;
  bool flip;
  int16_t X(float u) const { return (int16_t)lroundf(cx + (flip ? -u : u) * sc); }
  int16_t Y(float v) const { return (int16_t)lroundf(cy + v * sc); }
  float R(float r) const { return r * sc; }
  void rect(float u, float v, float w, float h, uint16_t c, uint8_t a = 255) const {
    float u0 = flip ? -(u + w) : u;
    s.fillRect((int16_t)lroundf(cx + u0 * sc), Y(v), (int16_t)lroundf(w * sc), (int16_t)lroundf(h * sc), c, a);
  }
  void rrect(float u, float v, float w, float h, float r, uint16_t c) const {
    float u0 = flip ? -(u + w) : u;
    s.fillRoundRect((int16_t)lroundf(cx + u0 * sc), Y(v), (int16_t)lroundf(w * sc), (int16_t)lroundf(h * sc),
                    (int16_t)lroundf(r * sc), c);
  }
  void tri(float u0, float v0, float u1, float v1, float u2, float v2, uint16_t c) const {
    s.fillTriangle(X(u0), Y(v0), X(u1), Y(v1), X(u2), Y(v2), c);
  }
  void circle(float u, float v, float r, uint16_t c, uint8_t a = 255) const { s.fillCircle(X(u), Y(v), R(r), c, a); }
  void line(float u0, float v0, float u1, float v1, uint16_t c) const { s.line(X(u0), Y(v0), X(u1), Y(v1), c); }
  // upper half of a disc (domes for caps and helmets)
  void dome(float u, float v, float r, uint16_t c) const {
    s.setClip(0, 0, 320, Y(v) + 1);
    s.fillCircle(X(u), Y(v), R(r), c);
    s.resetClip();
  }
};

const uint16_t kInk = hex(0x10151A);
}  // namespace

void drawHat(gfx::Surface& s, uint8_t id, float cx, float brimY, float sc, uint32_t now, bool flip) {
  Pen p{s, cx, brimY, sc, flip};
  float t = now / 1000.0f;
  switch (id) {
    case 1: {  // Party Hat
      p.tri(-11, 2, 11, 2, 2, -26, hex(0xFF4FD8));
      p.line(-6, -4, 6, -2, hex(0xFFE14D));
      p.line(-3, -12, 5, -10, hex(0x3BE8DA));
      p.line(-1, -19, 4, -18, hex(0xFFE14D));
      p.circle(2, -27, 3.2f, hex(0xFFE14D));
      break;
    }
    case 2: {  // Antenna Band
      p.rrect(-15, -1, 30, 5, 2, kInk);
      p.line(6, -1, 11, -20, hex(0x9AA5AE));
      bool on = ((now / 400) % 2) == 0;
      if (on) s.glow(p.X(11), p.Y(-21), p.R(7), hex(0x8EE34F), 140);
      p.circle(11, -21, 2.6f, on ? hex(0x8EE34F) : hex(0x2F5A20));
      break;
    }
    case 3: {  // Beanie
      p.dome(0, 3, 15, hex(0xD8463A));
      p.rect(-15, 0, 30, 4, hex(0xA62E26));
      for (int i = -12; i <= 12; i += 6) p.rect(i, -9, 2, 9, hex(0xEB6A5E));
      p.circle(0, -13, 3.5f, hex(0xF2F2F2));
      break;
    }
    case 4: {  // Propeller Cap
      p.dome(0, 3, 14, hex(0x3A7BFF));
      p.dome(0, 3, 14 * 0.5f, hex(0xFFD54A));
      p.rect(-14, 0, 30, 3, hex(0x1E4CB0));
      p.line(0, -11, 0, -15, kInk);
      float spin = cosf(t * 18);  // blades seen edge-on as they turn
      float len = 11 * spin;
      p.rrect(-fabsf(len), -17, 2 * fabsf(len) + 1, 3, 1, hex(0xFF4A3D));
      p.circle(0, -16, 1.8f, kInk);
      break;
    }
    case 5: {  // Pirate Bandana
      p.dome(0, 4, 15, hex(0xC8302A));
      for (int i = -9; i <= 9; i += 6) p.circle(i, -4, 1.3f, 0xFFFF);
      p.tri(13, 0, 21, 6, 16, 9, hex(0xC8302A));
      p.tri(13, 0, 23, 0, 20, 5, hex(0xA82520));
      break;
    }
    case 6: {  // Top Hat
      p.rect(-10, -22, 20, 22, kInk);
      p.rect(-10, -7, 20, 4, hex(0xC8302A));
      p.rrect(-17, -1, 34, 4, 2, kInk);
      p.rect(-7, -20, 2, 12, hex(0x39424A));
      break;
    }
    case 7: {  // Viking Helmet
      p.tri(-13, -3, -24, -14, -21, -24, hex(0xF1E9D2));
      p.tri(13, -3, 24, -14, 21, -24, hex(0xF1E9D2));
      p.dome(0, 3, 15, hex(0x9AA5AE));
      p.rect(-15, 0, 30, 4, hex(0x6C757D));
      p.rect(-1, -12, 2, 12, hex(0x6C757D));
      for (int i = -12; i <= 12; i += 6) p.circle(i, 2, 1, hex(0xD9DEE2));
      break;
    }
    case 8: {  // Crown
      uint16_t g = hex(0xFFD54A), gd = hex(0xC99A1C);
      p.rect(-14, -6, 28, 8, g);
      p.tri(-14, -6, -14, -17, -6, -6, g);
      p.tri(-6, -6, 0, -20, 6, -6, g);
      p.tri(6, -6, 14, -17, 14, -6, g);
      p.rect(-14, 0, 28, 2, gd);
      p.circle(0, -2, 2.2f, hex(0xFF4A3D));
      p.circle(-8, -2, 1.6f, hex(0x3BE8DA));
      p.circle(8, -2, 1.6f, hex(0x3BE8DA));
      float tw = 0.5f + 0.5f * sinf(t * 3);
      s.glow(p.X(0), p.Y(-20), p.R(5), 0xFFFF, (uint8_t)(150 * tw));
      break;
    }
    case 9: {  // Wizard Hat
      uint16_t c = hex(0x5B3FC4);
      p.tri(-13, 0, 13, 0, 5, -36, c);
      p.tri(5, -36, 9, -32, 14, -38, c);  // floppy tip
      p.rrect(-20, -1, 40, 5, 2, hex(0x4A33A3));
      float tw = 0.6f + 0.4f * sinf(t * 4);
      s.glow(p.X(1), p.Y(-14), p.R(6), hex(0xFFD54A), (uint8_t)(160 * tw));
      p.circle(1, -14, 2, hex(0xFFD54A));
      p.circle(-4, -6, 1, hex(0xFFD54A));
      break;
    }
    case 10: {  // Halo
      float bob = sinf(t * 2) * 1.5f;
      int16_t hx = p.X(0), hy = p.Y(-14 + bob);
      s.glow(hx, hy, p.R(18), hex(0xFFD54A), 70);
      for (int i = 0; i < 48; i++) {  // flattened ring
        float a = i * 6.2832f / 48;
        int16_t x = (int16_t)(hx + cosf(a) * p.R(13)), y = (int16_t)(hy + sinf(a) * p.R(4));
        s.fillRect(x - 1, y - 1, 2, 2, hex(0xFFE58A));
      }
      break;
    }
    case 11: {  // Chef Hat
      uint16_t w = hex(0xF4F6F8);
      p.rect(-11, -8, 22, 10, w);
      p.circle(-8, -12, 6, w);
      p.circle(0, -16, 7, w);
      p.circle(8, -12, 6, w);
      p.rect(-11, -1, 22, 3, hex(0xD5DADF));
      break;
    }
    case 12: {  // Tinfoil Hat
      uint16_t f = hex(0xC9D6E0), d = hex(0x8C9AA6);
      p.tri(-14, 2, 14, 2, 0, -24, f);
      p.line(-6, -2, -1, -12, d);
      p.line(4, -3, 1, -16, d);
      p.line(-9, -1, 9, 0, d);
      p.line(-3, -8, 6, -6, d);
      float sh = 0.5f + 0.5f * sinf(t * 5);
      s.glow(p.X(-4), p.Y(-10), p.R(4), 0xFFFF, (uint8_t)(170 * sh));
      break;
    }
    case 13: {  // Bee Antennae
      float wob = sinf(t * 6) * 2;
      p.line(-5, 0, -10 + wob, -18, kInk);
      p.line(5, 0, 10 - wob, -18, kInk);
      p.circle(-10 + wob, -19, 3, hex(0xFFD54A));
      p.circle(10 - wob, -19, 3, hex(0xFFD54A));
      p.rect(-12 + wob, -20, 4, 1.5f, kInk);
      p.rect(8 - wob, -20, 4, 1.5f, kInk);
      break;
    }
    case 14: {  // Heart Band: two hearts bobbing on springs
      p.rrect(-15, -1, 30, 4, 2, hex(0xFF4F8B));
      for (int side = -1; side <= 1; side += 2) {
        float bob = sinf(t * 5 + side) * 2;
        float hx = side * 8, hy = -18 + bob;
        p.line(side * 6, -1, hx, hy + 3, hex(0x9AA5AE));
        uint16_t c = hex(0xFF3D6E);
        p.circle(hx - 2.4f, hy - 1, 2.8f, c);
        p.circle(hx + 2.4f, hy - 1, 2.8f, c);
        p.tri(hx - 5.1f, hy, hx + 5.1f, hy, hx, hy + 6, c);
      }
      break;
    }
    case 15: {  // Bunny Ears
      float wob = sinf(t * 3) * 1.5f;
      uint16_t w = hex(0xF4F1EC), pink = hex(0xFFB3C8);
      p.rrect(-11 + wob * 0.3f, -30, 8, 31, 4, w);
      p.rrect(3 - wob * 0.3f, -28, 8, 29, 4, w);
      p.rrect(-9 + wob * 0.3f, -26, 4, 22, 2, pink);
      p.rrect(5 - wob * 0.3f, -24, 4, 20, 2, pink);
      p.rrect(-14, -1, 28, 4, 2, hex(0xD5DADF));
      break;
    }
    case 16: {  // Witch Hat
      uint16_t c = hex(0x2A1F3D), band = hex(0xFF8A1F);
      p.tri(-11, -2, 11, -2, 6, -30, c);
      p.tri(6, -30, 10, -27, 16, -33, c);  // bent tip
      p.rect(-10, -7, 20, 5, band);
      p.rect(-3, -7, 5, 5, hex(0xFFD54A));
      p.rect(-2, -6, 3, 3, c);
      p.rrect(-22, -2, 44, 5, 2, c);
      float tw = 0.5f + 0.5f * sinf(t * 4);
      s.glow(p.X(16), p.Y(-33), p.R(4), hex(0xB46CFF), (uint8_t)(150 * tw));
      break;
    }
    case 17: {  // Santa Hat
      uint16_t red = hex(0xD8263A), fur = hex(0xF7F7F7);
      float sway = sinf(t * 2) * 1.5f;
      p.tri(-13, 0, 13, 0, 14 + sway, -22, red);
      p.tri(4, -14, 14 + sway, -22, 20 + sway, -8, red);  // floppy end
      p.circle(20 + sway, -7, 4, fur);
      p.rrect(-16, -3, 32, 7, 3, fur);
      break;
    }
    case 18: {  // Black Hat: a hacker's fedora with a terminal-green band
      uint16_t felt = hex(0x262C33), edge = hex(0x5A6670), green = hex(0x8EE34F);
      p.rrect(-22, -3, 44, 5, 2, felt);
      p.rrect(-12, -17, 24, 15, 6, felt);
      p.tri(-5, -17, 5, -17, 0, -13, edge);  // the pinch on top
      p.rect(-11, -7, 22, 4, green);
      p.rect(-9, -15, 2, 7, edge);           // highlight so it shows on dark backgrounds
      p.line(-21, -3, 21, -3, edge);
      float blink = 0.5f + 0.5f * sinf(t * 3);
      s.glow(p.X(0), p.Y(-5), p.R(9), green, (uint8_t)(70 * blink));
      break;
    }
    default: break;
  }
}

}  // namespace ui
