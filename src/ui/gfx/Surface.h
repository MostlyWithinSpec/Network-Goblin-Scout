#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Font.h"

// Small software renderer that draws straight into an RGB565 framebuffer, with alpha
// blending and anti-aliasing (which Arduino_GFX doesn't do). Pure C++: the same code
// renders on the device and in the PC preview tool (tools/preview).
namespace gfx {

inline constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
inline constexpr uint16_t hex(uint32_t c) { return rgb((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF); }

// a = 0..255 coverage of src. RGB565 "spread" trick: green moves to the top half of a
// 32-bit word, leaving spare bits above each channel so all three blend in one multiply.
__attribute__((always_inline)) inline uint16_t blend(uint16_t d, uint16_t s, uint8_t a) {
  if (a >= 252) return s;
  if (a < 4) return d;
  uint32_t al = (a + 4) >> 3;  // 0..32
  uint32_t D = (d | ((uint32_t)d << 16)) & 0x07E0F81F;
  uint32_t S = (s | ((uint32_t)s << 16)) & 0x07E0F81F;
  uint32_t R = ((S * al + D * (32 - al)) >> 5) & 0x07E0F81F;
  return (uint16_t)(R | (R >> 16));
}
uint16_t mix(uint16_t a, uint16_t b, uint8_t t);         // t = 0 -> a, 255 -> b
uint16_t dim(uint16_t c, uint8_t k);                     // k = 255 -> unchanged
uint16_t hue(uint8_t h, uint8_t s = 255, uint8_t v = 255);  // HSV with 0-255 ranges
uint8_t luma(uint16_t c);

// Options for drawing a sprite (RGB565 + 8-bit alpha + optional pixel classes).
struct SpriteFx {
  float scaleX = 1, scaleY = 1;
  bool flipX = false;
  uint8_t alpha = 255;            // overall opacity
  uint8_t bright = 255;           // 255 = normal, lower = darker
  uint16_t skinTint = 0;          // class 1: recolour keeping shading (0 = off)
  bool skinTintOn = false;
  uint16_t eyeColor = 0;          // class 2
  uint8_t eyeMix = 0;             //   how much of eyeColor (0 = original)
  uint16_t screenColor = 0;       // class 3
  uint8_t screenMix = 0;
  uint16_t flash = 0;             // whole sprite toward this colour
  uint8_t flashMix = 0;
};

class Surface {
 public:
  Surface(uint16_t* px, int16_t w, int16_t h) : px_(px), w_(w), h_(h) { resetClip(); }
  int16_t width() const { return w_; }
  int16_t height() const { return h_; }
  uint16_t* pixels() { return px_; }

  void setClip(int16_t x, int16_t y, int16_t w, int16_t h);
  void resetClip() { cx0_ = 0; cy0_ = 0; cx1_ = w_; cy1_ = h_; }
  void offset(int16_t dx, int16_t dy) { ox_ = dx; oy_ = dy; }  // translate all drawing (transitions)

  void clear(uint16_t c);
  __attribute__((always_inline)) inline void pixel(int16_t x, int16_t y, uint16_t c, uint8_t a = 255) {
    x += ox_;
    y += oy_;
    if (x < cx0_ || x >= cx1_ || y < cy0_ || y >= cy1_ || !a) return;
    uint16_t& p = px_[(size_t)y * w_ + x];
    p = blend(p, c, a);
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint8_t a = 255);
  void rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint8_t a = 255);
  void hline(int16_t x, int16_t y, int16_t w, uint16_t c, uint8_t a = 255) { fillRect(x, y, w, 1, c, a); }
  void vline(int16_t x, int16_t y, int16_t h, uint16_t c, uint8_t a = 255) { fillRect(x, y, 1, h, c, a); }
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c, uint8_t a = 255);
  void gradientV(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t top, uint16_t bottom, uint8_t a = 255);
  void gradientH(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t left, uint16_t right, uint8_t a = 255);
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c, uint8_t a = 255);
  void roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c, uint8_t a = 255);
  // Anti-aliased disc / ring / arc. Angles in degrees, 0 = 12 o'clock, clockwise.
  void fillCircle(float cx, float cy, float r, uint16_t c, uint8_t a = 255);
  void ring(float cx, float cy, float r, float thick, uint16_t c, uint8_t a = 255, float startDeg = 0,
            float sweepDeg = 360);
  // Arc from 12 o'clock, colour fading c0 -> c1 along the sweep (single pass).
  void ringGradient(float cx, float cy, float r, float thick, uint16_t c0, uint16_t c1, float sweepDeg,
                    uint8_t a = 255);
  void glow(float cx, float cy, float r, uint16_t c, uint8_t a);  // soft radial light
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t c,
                    uint8_t a = 255);

  void sprite(const uint16_t* px, const uint8_t* alpha, const uint8_t* cls, int16_t w, int16_t h, int16_t x,
              int16_t y, const SpriteFx& fx = SpriteFx());
  void alphaMask(const uint8_t* mask, int16_t w, int16_t h, int16_t x, int16_t y, uint16_t c, uint8_t a = 255);
  void blitKeyed(const uint16_t* px, int16_t w, int16_t h, int16_t x, int16_t y, uint16_t key, int scale = 1);

  // Text. y is the top of the line box. Returns the x after the last glyph.
  int16_t text(const Font& f, int16_t x, int16_t y, const char* s, uint16_t c, uint8_t a = 255);
  int16_t textCentered(const Font& f, int16_t cx, int16_t y, const char* s, uint16_t c, uint8_t a = 255);
  int16_t textRight(const Font& f, int16_t rx, int16_t y, const char* s, uint16_t c, uint8_t a = 255);
  static int16_t textWidth(const Font& f, const char* s);

 private:
  uint16_t* px_;
  int16_t w_, h_;
  int16_t cx0_, cy0_, cx1_, cy1_;
  int16_t ox_ = 0, oy_ = 0;
  void span(int16_t x0, int16_t x1, int16_t y, uint16_t c, uint8_t a);  // clipped, screen coords
};

}  // namespace gfx
