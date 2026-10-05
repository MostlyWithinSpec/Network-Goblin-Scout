#include "Surface.h"
#include <math.h>

namespace gfx {

// RGB565 blend using the "spread" trick: green moves to the top half of a 32-bit word,
// leaving 5+ spare bits above each channel so all three blend in one multiply.
uint16_t blend(uint16_t d, uint16_t s, uint8_t a) {
  if (a >= 252) return s;
  if (a < 4) return d;
  uint32_t al = (a + 4) >> 3;  // 0..32
  uint32_t D = (d | ((uint32_t)d << 16)) & 0x07E0F81F;
  uint32_t S = (s | ((uint32_t)s << 16)) & 0x07E0F81F;
  uint32_t R = ((S * al + D * (32 - al)) >> 5) & 0x07E0F81F;
  return (uint16_t)(R | (R >> 16));
}

uint16_t mix(uint16_t a, uint16_t b, uint8_t t) { return blend(a, b, t); }
uint16_t dim(uint16_t c, uint8_t k) { return blend(0, c, k); }

uint16_t hue(uint8_t h, uint8_t s, uint8_t v) {
  uint8_t region = h / 43, rem = (uint8_t)((h - region * 43) * 6);
  uint8_t p = (uint8_t)((v * (255 - s)) >> 8);
  uint8_t q = (uint8_t)((v * (255 - ((s * rem) >> 8))) >> 8);
  uint8_t t = (uint8_t)((v * (255 - ((s * (255 - rem)) >> 8))) >> 8);
  switch (region) {
    case 0: return rgb(v, t, p);
    case 1: return rgb(q, v, p);
    case 2: return rgb(p, v, t);
    case 3: return rgb(p, q, v);
    case 4: return rgb(t, p, v);
    default: return rgb(v, p, q);
  }
}

uint8_t luma(uint16_t c) {
  uint32_t r = (c >> 11) << 3, g = ((c >> 5) & 0x3F) << 2, b = (c & 0x1F) << 3;
  return (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
}

static inline uint8_t maxChannel(uint16_t c) {
  uint8_t r = (uint8_t)((c >> 11) << 3), g = (uint8_t)(((c >> 5) & 0x3F) << 2), b = (uint8_t)((c & 0x1F) << 3);
  uint8_t m = r > g ? r : g;
  return m > b ? m : b;
}

static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// ---------------------------------------------------------------------------
void Surface::setClip(int16_t x, int16_t y, int16_t w, int16_t h) {
  x += ox_;
  y += oy_;
  cx0_ = x < 0 ? 0 : x;
  cy0_ = y < 0 ? 0 : y;
  cx1_ = x + w > w_ ? w_ : x + w;
  cy1_ = y + h > h_ ? h_ : y + h;
}

void Surface::clear(uint16_t c) {
  size_t n = (size_t)w_ * h_;
  for (size_t i = 0; i < n; i++) px_[i] = c;
}

void Surface::span(int16_t x0, int16_t x1, int16_t y, uint16_t c, uint8_t a) {
  if (y < cy0_ || y >= cy1_ || !a) return;
  if (x0 < cx0_) x0 = cx0_;
  if (x1 > cx1_) x1 = cx1_;
  uint16_t* p = px_ + (size_t)y * w_;
  if (a >= 252)
    for (int16_t x = x0; x < x1; x++) p[x] = c;
  else
    for (int16_t x = x0; x < x1; x++) p[x] = blend(p[x], c, a);
}

void Surface::pixel(int16_t x, int16_t y, uint16_t c, uint8_t a) {
  x += ox_;
  y += oy_;
  if (x < cx0_ || x >= cx1_ || y < cy0_ || y >= cy1_ || !a) return;
  uint16_t& p = px_[(size_t)y * w_ + x];
  p = blend(p, c, a);
}

void Surface::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint8_t a) {
  x += ox_;
  y += oy_;
  for (int16_t j = 0; j < h; j++) span(x, x + w, y + j, c, a);
}

void Surface::rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c, uint8_t a) {
  hline(x, y, w, c, a);
  hline(x, y + h - 1, w, c, a);
  vline(x, y + 1, h - 2, c, a);
  vline(x + w - 1, y + 1, h - 2, c, a);
}

void Surface::line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c, uint8_t a) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
  int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    pixel(x0, y0, c, a);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void Surface::gradientV(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t top, uint16_t bottom, uint8_t a) {
  for (int16_t j = 0; j < h; j++)
    fillRect(x, y + j, w, 1, mix(top, bottom, (uint8_t)(h > 1 ? j * 255 / (h - 1) : 0)), a);
}

void Surface::gradientH(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t left, uint16_t right, uint8_t a) {
  for (int16_t i = 0; i < w; i++)
    fillRect(x + i, y, 1, h, mix(left, right, (uint8_t)(w > 1 ? i * 255 / (w - 1) : 0)), a);
}

void Surface::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c, uint8_t a) {
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  for (int16_t j = 0; j < h; j++) {
    float inset = 0;
    float cy = j < r ? r - j - 0.5f : (j >= h - r ? j - (h - r) + 0.5f : 0);
    if (cy > 0) inset = r - sqrtf(fmaxf(0, (float)r * r - cy * cy));
    int16_t full = (int16_t)ceilf(inset);
    float cov = full - inset;
    int16_t sy = y + j + oy_;
    span(x + ox_ + full, x + ox_ + w - full, sy, c, a);
    if (full > 0 && cov > 0.02f) {
      uint8_t ea = (uint8_t)(a * cov);
      span(x + ox_ + full - 1, x + ox_ + full, sy, c, ea);
      span(x + ox_ + w - full, x + ox_ + w - full + 1, sy, c, ea);
    }
  }
}

void Surface::roundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c, uint8_t a) {
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  hline(x + r, y, w - 2 * r, c, a);
  hline(x + r, y + h - 1, w - 2 * r, c, a);
  vline(x, y + r, h - 2 * r, c, a);
  vline(x + w - 1, y + r, h - 2 * r, c, a);
  if (r <= 0) return;
  float rr = r - 0.5f;
  ring(x + r, y + r, rr + 0.5f, 1, c, a, 270, 90);
  ring(x + w - r, y + r, rr + 0.5f, 1, c, a, 0, 90);
  ring(x + w - r, y + h - r, rr + 0.5f, 1, c, a, 90, 90);
  ring(x + r, y + h - r, rr + 0.5f, 1, c, a, 180, 90);
}

void Surface::fillCircle(float cx, float cy, float r, uint16_t c, uint8_t a) {
  if (r <= 0) return;
  int16_t y0 = (int16_t)floorf(cy - r - 1), y1 = (int16_t)ceilf(cy + r + 1);
  for (int16_t y = y0; y <= y1; y++) {
    float dy = y + 0.5f - cy;
    float h2 = (r + 0.5f) * (r + 0.5f) - dy * dy;
    if (h2 <= 0) continue;
    float half = sqrtf(h2);
    int16_t xa = (int16_t)floorf(cx - half), xb = (int16_t)ceilf(cx + half);
    // fully covered middle: pixels whose centre is at least 0.5 px inside
    float in2 = (r - 0.5f) * (r - 0.5f) - dy * dy;
    int16_t ia = xb, ib = xa;
    if (in2 > 0) {
      float ih = sqrtf(in2);
      ia = (int16_t)ceilf(cx - ih - 0.5f);
      ib = (int16_t)floorf(cx + ih - 0.5f);
      if (ib >= ia) span(ia + ox_, ib + 1 + ox_, y + oy_, c, a);
    }
    for (int16_t x = xa; x <= xb; x++) {  // anti-aliased edges only
      if (x >= ia && x <= ib) { x = ib; continue; }
      float dx = x + 0.5f - cx;
      float cov = clamp01(r - sqrtf(dx * dx + dy * dy) + 0.5f);
      if (cov > 0) pixel(x, y, c, (uint8_t)(a * cov));
    }
  }
}

// Visits only the pixels of the annulus (not the whole bounding box).
template <typename F>
static void annulus(float cx, float cy, float r, float inner, F&& fn) {
  int16_t y0 = (int16_t)floorf(cy - r - 1), y1 = (int16_t)ceilf(cy + r + 1);
  float ro = r + 0.5f, ri = inner - 0.5f;
  for (int16_t y = y0; y <= y1; y++) {
    float dy = y + 0.5f - cy;
    float h2 = ro * ro - dy * dy;
    if (h2 <= 0) continue;
    float half = sqrtf(h2);
    float ih = (ri > 0 && ri * ri > dy * dy) ? sqrtf(ri * ri - dy * dy) : -1;
    int16_t xa = (int16_t)floorf(cx - half), xb = (int16_t)ceilf(cx + half);
    for (int16_t x = xa; x <= xb; x++) {
      float dx = x + 0.5f - cx;
      if (ih > 0 && dx > -ih + 1 && dx < ih - 1) {  // skip the hole
        x = (int16_t)floorf(cx + ih - 1);
        continue;
      }
      fn(x, y, dx, dy);
    }
  }
}

static inline bool inSweep(float dx, float dy, float startDeg, float sweepDeg, float& rel) {
  float ang = atan2f(dx, -dy) * 57.29578f;
  if (ang < 0) ang += 360;
  rel = ang - startDeg;
  while (rel < 0) rel += 360;
  while (rel >= 360) rel -= 360;
  return rel <= sweepDeg;
}

void Surface::ring(float cx, float cy, float r, float thick, uint16_t c, uint8_t a, float startDeg,
                   float sweepDeg) {
  if (sweepDeg <= 0) return;
  bool full = sweepDeg >= 360;
  float inner = r - thick;
  annulus(cx, cy, r, inner, [&](int16_t x, int16_t y, float dx, float dy) {
    float d = sqrtf(dx * dx + dy * dy);
    float cov = clamp01(fminf(r - d + 0.5f, d - inner + 0.5f));
    if (cov <= 0) return;
    float rel;
    if (!full && !inSweep(dx, dy, startDeg, sweepDeg, rel)) return;
    pixel(x, y, c, (uint8_t)(a * cov));
  });
}

void Surface::ringGradient(float cx, float cy, float r, float thick, uint16_t c0, uint16_t c1, float sweepDeg,
                           uint8_t a) {
  if (sweepDeg <= 0) return;
  float inner = r - thick;
  annulus(cx, cy, r, inner, [&](int16_t x, int16_t y, float dx, float dy) {
    float d = sqrtf(dx * dx + dy * dy);
    float cov = clamp01(fminf(r - d + 0.5f, d - inner + 0.5f));
    if (cov <= 0) return;
    float rel;
    if (!inSweep(dx, dy, 0, sweepDeg, rel)) return;
    pixel(x, y, mix(c0, c1, (uint8_t)(rel / sweepDeg * 255)), (uint8_t)(a * cov));
  });
}

void Surface::glow(float cx, float cy, float r, uint16_t c, uint8_t a) {
  int16_t x0 = (int16_t)(cx - r), x1 = (int16_t)(cx + r), y0 = (int16_t)(cy - r), y1 = (int16_t)(cy + r);
  float inv = 1.0f / r;
  for (int16_t y = y0; y <= y1; y++) {
    float dy = (y + 0.5f - cy) * inv;
    for (int16_t x = x0; x <= x1; x++) {
      float dx = (x + 0.5f - cx) * inv;
      float t = 1 - (dx * dx + dy * dy);
      if (t > 0) pixel(x, y, c, (uint8_t)(a * t * t));
    }
  }
}

void Surface::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t c,
                           uint8_t a) {
  // sort by y
  if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y1 > y2) { int16_t t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
  if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
  if (y2 == y0) return;
  for (int16_t y = y0; y <= y2; y++) {
    float xa = x0 + (float)(x2 - x0) * (y - y0) / (y2 - y0);
    float xb = (y < y1) ? (y1 == y0 ? x1 : x0 + (float)(x1 - x0) * (y - y0) / (y1 - y0))
                        : (y2 == y1 ? x1 : x1 + (float)(x2 - x1) * (y - y1) / (y2 - y1));
    if (xa > xb) { float t = xa; xa = xb; xb = t; }
    span((int16_t)lroundf(xa) + ox_, (int16_t)lroundf(xb) + 1 + ox_, y + oy_, c, a);
  }
}

// ---------------------------------------------------------------------------
void Surface::sprite(const uint16_t* px, const uint8_t* alpha, const uint8_t* cls, int16_t w, int16_t h, int16_t x,
                     int16_t y, const SpriteFx& fx) {
  int16_t dw = (int16_t)lroundf(w * fx.scaleX), dh = (int16_t)lroundf(h * fx.scaleY);
  if (dw <= 0 || dh <= 0) return;
  float ix = (float)w / dw, iy = (float)h / dh;
  for (int16_t j = 0; j < dh; j++) {
    int16_t sy = (int16_t)(j * iy);
    if (sy >= h) sy = h - 1;
    for (int16_t i = 0; i < dw; i++) {
      int16_t sx = (int16_t)(i * ix);
      if (sx >= w) sx = w - 1;
      if (fx.flipX) sx = w - 1 - sx;
      size_t idx = (size_t)sy * w + sx;
      uint8_t a = alpha[idx];
      if (!a) continue;
      uint16_t c = px[idx];
      uint8_t k = cls ? cls[idx] : 0;
      if (k == 1 && fx.skinTintOn) {
        uint32_t l = maxChannel(c) * 255u / 170u;  // keep the shading of the original green
        c = dim(fx.skinTint, (uint8_t)(l > 255 ? 255 : l));
      } else if (k == 2 && fx.eyeMix) {
        c = mix(c, fx.eyeColor, fx.eyeMix);
      } else if (k == 3 && fx.screenMix) {
        c = mix(c, fx.screenColor, fx.screenMix);
      }
      if (fx.bright != 255) c = dim(c, fx.bright);
      if (fx.flashMix) c = mix(c, fx.flash, fx.flashMix);
      pixel(x + i, y + j, c, (uint8_t)(a * fx.alpha / 255));
    }
  }
}

void Surface::alphaMask(const uint8_t* mask, int16_t w, int16_t h, int16_t x, int16_t y, uint16_t c, uint8_t a) {
  for (int16_t j = 0; j < h; j++)
    for (int16_t i = 0; i < w; i++) {
      uint8_t m = mask[(size_t)j * w + i];
      if (m) pixel(x + i, y + j, c, (uint8_t)(m * a / 255));
    }
}

void Surface::blitKeyed(const uint16_t* px, int16_t w, int16_t h, int16_t x, int16_t y, uint16_t key, int scale) {
  for (int16_t j = 0; j < h * scale; j++)
    for (int16_t i = 0; i < w * scale; i++) {
      uint16_t c = px[(size_t)(j / scale) * w + i / scale];
      if (c != key) pixel(x + i, y + j, c);
    }
}

// ---------------------------------------------------------------------------
int16_t Surface::text(const Font& f, int16_t x, int16_t y, const char* s, uint16_t c, uint8_t a) {
  for (; *s; s++) {
    uint8_t ch = (uint8_t)*s;
    if (ch < f.first || ch > f.last) ch = '?';
    const Glyph& g = f.glyphs[ch - f.first];
    const uint8_t* bits = f.bits + g.offset;
    for (int16_t j = 0; j < g.h; j++)
      for (int16_t i = 0; i < g.w; i++) {
        size_t n = (size_t)j * g.w + i;
        uint8_t v = (n & 1) ? (bits[n >> 1] & 0x0F) : (bits[n >> 1] >> 4);
        if (v) pixel(x + g.xoff + i, y + g.yoff + j, c, (uint8_t)(v * 17 * a / 255));
      }
    x += g.adv;
  }
  return x;
}

int16_t Surface::textWidth(const Font& f, const char* s) {
  int16_t w = 0;
  for (; *s; s++) {
    uint8_t ch = (uint8_t)*s;
    if (ch < f.first || ch > f.last) ch = '?';
    w += f.glyphs[ch - f.first].adv;
  }
  return w;
}

int16_t Surface::textCentered(const Font& f, int16_t cx, int16_t y, const char* s, uint16_t c, uint8_t a) {
  return text(f, cx - textWidth(f, s) / 2, y, s, c, a);
}

int16_t Surface::textRight(const Font& f, int16_t rx, int16_t y, const char* s, uint16_t c, uint8_t a) {
  return text(f, rx - textWidth(f, s), y, s, c, a);
}

}  // namespace gfx
