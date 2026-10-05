#include "Surface.h"
#include <math.h>

namespace gfx {

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

// ---------------------------------------------------------------------------
// The ESP32-C5 has no FPU, so everything below runs per pixel in integers only.
// Coordinates are fixed point with 4 fractional bits (1/16 px); floats appear only
// once per call when converting the arguments.

static inline int32_t fx16(float v) { return (int32_t)lroundf(v * 16); }

static uint32_t isqrt(uint32_t v) {
  uint32_t r = 0, b = 1u << 30;
  while (b > v) b >>= 2;
  while (b) {
    if (v >= r + b) { v -= r + b; r = (r >> 1) + b; }
    else r >>= 1;
    b >>= 2;
  }
  return r;
}

static inline int32_t floorDiv16(int32_t v) { return v >= 0 ? v >> 4 : -((-v + 15) >> 4); }

// "Diamond angle": a cheap, monotonic stand-in for the angle of (x, y), with y pointing
// up. 0..4095 = one turn, 0 at 12 o'clock, increasing clockwise. No trig needed.
static inline int32_t pang(int32_t x, int32_t y) {
  int32_t d;
  if (y >= 0) d = (x >= 0) ? (x + y ? y * 1024 / (x + y) : 0) : 1024 + (-x) * 1024 / (-x + y);
  else d = (x < 0) ? 2048 + (-y) * 1024 / (-x - y) : 3072 + x * 1024 / (x - y);
  return (1024 - d) & 4095;
}
static int32_t pangDeg(float deg) {
  int32_t q = (int32_t)deg;
  if ((float)q == deg && q % 90 == 0) return ((q / 90) * 1024) & 4095;  // no trig for 0/90/180/270
  float r = deg * 0.01745329f;
  return pang((int32_t)lroundf(sinf(r) * 4096), (int32_t)lroundf(cosf(r) * 4096));
}

// Visits the pixels of an annulus (skipping the hole) and hands each one its coverage
// (0..255) and its diamond angle, all in integers.
template <typename F>
static void annulusFixed(int32_t cx, int32_t cy, int32_t r, int32_t in, F&& fn) {
  int32_t ro = r + 8, roi = r - 8, ii = in - 8, io = in + 8;
  int32_t ro2 = ro * ro, roi2 = roi > 0 ? roi * roi : 0;
  int32_t ii2 = ii > 0 ? ii * ii : 0, io2 = io > 0 ? io * io : 0;
  int32_t outerBand = ro2 - roi2 > 0 ? ro2 - roi2 : 1, innerBand = io2 - ii2 > 0 ? io2 - ii2 : 1;
  int16_t y0 = (int16_t)floorDiv16(cy - ro), y1 = (int16_t)floorDiv16(cy + ro);
  for (int16_t y = y0; y <= y1; y++) {
    int32_t dy = y * 16 + 8 - cy, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    int32_t ho = (int32_t)isqrt((uint32_t)(ro2 - dy2));
    int16_t xa = (int16_t)floorDiv16(cx - ho), xb = (int16_t)floorDiv16(cx + ho);
    int16_t ha = xb + 1, hb = xa - 1;  // pixels entirely in the hole
    if (ii2 > dy2) {
      int32_t hh = (int32_t)isqrt((uint32_t)(ii2 - dy2));
      ha = (int16_t)floorDiv16(cx - hh - 8 + 15);
      hb = (int16_t)floorDiv16(cx + hh - 8);
    }
    for (int16_t x = xa; x <= xb; x++) {
      if (x >= ha && x <= hb) { x = hb; continue; }
      int32_t dx = x * 16 + 8 - cx, d2 = dx * dx + dy2;
      if (d2 >= ro2 || d2 <= ii2) continue;
      int32_t co = d2 <= roi2 ? 255 : (ro2 - d2) * 255 / outerBand;
      int32_t ci = d2 >= io2 ? 255 : (d2 - ii2) * 255 / innerBand;
      int32_t cov = co < ci ? co : ci;
      if (cov > 0) fn(x, y, dx, dy, cov);
    }
  }
}

template <typename F>
static void annulus(float cxf, float cyf, float rf, float thickf, F&& fn) {
  annulusFixed(fx16(cxf), fx16(cyf), fx16(rf), fx16(rf - thickf), fn);
}

void Surface::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t c, uint8_t a) {
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  int32_t r16 = r * 16;
  for (int16_t j = 0; j < h; j++) {
    int32_t cy16 = j < r ? r16 - j * 16 - 8 : (j >= h - r ? (j - (h - r)) * 16 + 8 : 0);
    int32_t inset16 = 0;
    if (cy16 > 0) inset16 = r16 - (int32_t)isqrt((uint32_t)(r16 * r16 - cy16 * cy16 > 0 ? r16 * r16 - cy16 * cy16 : 0));
    int16_t full = (int16_t)((inset16 + 15) >> 4);
    int32_t cov = full * 16 - inset16;  // 0..15
    int16_t sy = y + j + oy_;
    span(x + ox_ + full, x + ox_ + w - full, sy, c, a);
    if (full > 0 && cov > 0) {
      uint8_t ea = (uint8_t)(a * cov / 16);
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
  // corners: 1 px quarter rings, all integer (quadrant starts 270, 0, 90, 180 degrees)
  const int16_t cxs[4] = {(int16_t)(x + r), (int16_t)(x + w - r), (int16_t)(x + w - r), (int16_t)(x + r)};
  const int16_t cys[4] = {(int16_t)(y + r), (int16_t)(y + r), (int16_t)(y + h - r), (int16_t)(y + h - r)};
  const int32_t starts[4] = {3072, 0, 1024, 2048};
  for (int q = 0; q < 4; q++) {
    int32_t s0 = starts[q];
    annulusFixed(cxs[q] * 16, cys[q] * 16, r * 16, (r - 1) * 16,
                 [&](int16_t px, int16_t py, int32_t dx, int32_t dy, int32_t cov) {
                   if (((pang(dx, -dy) - s0) & 4095) > 1024) return;
                   pixel(px, py, c, (uint8_t)(a * cov / 255));
                 });
  }
}

void Surface::fillCircle(float cxf, float cyf, float rf, uint16_t c, uint8_t a) {
  if (rf <= 0) return;
  int32_t cx = fx16(cxf), cy = fx16(cyf), r = fx16(rf);
  int32_t ro = r + 8, ri = r - 8;                 // AA band: +-0.5 px around the edge
  int32_t ro2 = ro * ro, ri2 = ri > 0 ? ri * ri : 0, band = ro2 - ri2;
  int16_t y0 = (int16_t)floorDiv16(cy - ro), y1 = (int16_t)floorDiv16(cy + ro);
  for (int16_t y = y0; y <= y1; y++) {
    int32_t dy = y * 16 + 8 - cy, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    int32_t ho = (int32_t)isqrt((uint32_t)(ro2 - dy2));
    int16_t xa = (int16_t)floorDiv16(cx - ho), xb = (int16_t)floorDiv16(cx + ho);
    int16_t ia = xb + 1, ib = xa - 1;           // fully covered range (pixel centres inside ri)
    if (ri2 > dy2) {
      int32_t hi = (int32_t)isqrt((uint32_t)(ri2 - dy2));
      ia = (int16_t)floorDiv16(cx - hi - 8 + 15);
      ib = (int16_t)floorDiv16(cx + hi - 8);
      if (ib >= ia) span(ia + ox_, ib + 1 + ox_, y + oy_, c, a);
    }
    for (int16_t x = xa; x <= xb; x++) {
      if (x >= ia && x <= ib) { x = ib; continue; }
      int32_t dx = x * 16 + 8 - cx, d2 = dx * dx + dy2;
      if (d2 >= ro2) continue;
      int32_t cov = d2 <= ri2 ? 255 : (ro2 - d2) * 255 / band;
      pixel(x, y, c, (uint8_t)(a * cov / 255));
    }
  }
}

void Surface::ring(float cx, float cy, float r, float thick, uint16_t c, uint8_t a, float startDeg,
                   float sweepDeg) {
  if (sweepDeg <= 0) return;
  bool full = sweepDeg >= 359.5f;
  int32_t s0 = full ? 0 : pangDeg(startDeg);
  int32_t sw = full ? 4096 : (pangDeg(startDeg + sweepDeg) - s0) & 4095;
  if (!full && sw == 0) sw = 4096;
  annulus(cx, cy, r, thick, [&](int16_t x, int16_t y, int32_t dx, int32_t dy, int32_t cov) {
    if (!full && ((pang(dx, -dy) - s0) & 4095) > sw) return;
    pixel(x, y, c, (uint8_t)(a * cov / 255));
  });
}

void Surface::ringGradient(float cx, float cy, float r, float thick, uint16_t c0, uint16_t c1, float sweepDeg,
                           uint8_t a) {
  if (sweepDeg <= 0) return;
  int32_t sw = sweepDeg >= 359.5f ? 4095 : pangDeg(sweepDeg);
  if (sw == 0) return;
  annulus(cx, cy, r, thick, [&](int16_t x, int16_t y, int32_t dx, int32_t dy, int32_t cov) {
    int32_t p = pang(dx, -dy);
    if (p > sw) return;
    pixel(x, y, mix(c0, c1, (uint8_t)(p * 255 / sw)), (uint8_t)(a * cov / 255));
  });
}

void Surface::glow(float cxf, float cyf, float rf, uint16_t c, uint8_t a) {
  if (rf <= 0 || !a) return;
  int32_t cx = fx16(cxf), cy = fx16(cyf), r = fx16(rf), r2 = r * r;
  int16_t y0 = (int16_t)floorDiv16(cy - r), y1 = (int16_t)floorDiv16(cy + r);
  for (int16_t y = y0; y <= y1; y++) {
    int32_t dy = y * 16 + 8 - cy, dy2 = dy * dy;
    if (dy2 >= r2) continue;
    int32_t hw = (int32_t)isqrt((uint32_t)(r2 - dy2));
    int16_t xa = (int16_t)floorDiv16(cx - hw), xb = (int16_t)floorDiv16(cx + hw);
    for (int16_t x = xa; x <= xb; x++) {
      int32_t dx = x * 16 + 8 - cx, d2 = dx * dx + dy2;
      if (d2 >= r2) continue;
      uint32_t t = (uint32_t)(r2 - d2) * 255u / (uint32_t)r2;  // 1 - d^2/r^2, 0..255
      uint32_t k = a * t * t / 65025u;
      if (k) pixel(x, y, c, (uint8_t)k);
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
  // clip rows to the visible area first; rays are mostly off-screen
  int16_t ya = y0, yb = y2;
  if (ya < cy0_ - oy_) ya = cy0_ - oy_;
  if (yb > cy1_ - 1 - oy_) yb = cy1_ - 1 - oy_;
  for (int16_t y = ya; y <= yb; y++) {
    // 16.16 fixed-point edge positions
    int32_t xa = (x0 << 16) + (int32_t)(((int64_t)(x2 - x0) << 16) * (y - y0) / (y2 - y0));
    int32_t xb;
    if (y < y1) xb = y1 == y0 ? (x1 << 16) : (x0 << 16) + (int32_t)(((int64_t)(x1 - x0) << 16) * (y - y0) / (y1 - y0));
    else xb = y2 == y1 ? (x1 << 16) : (x1 << 16) + (int32_t)(((int64_t)(x2 - x1) << 16) * (y - y1) / (y2 - y1));
    if (xa > xb) { int32_t t = xa; xa = xb; xb = t; }
    span((int16_t)((xa + 0x8000) >> 16) + ox_, (int16_t)((xb + 0x8000) >> 16) + 1 + ox_, y + oy_, c, a);
  }
}

// ---------------------------------------------------------------------------
void Surface::sprite(const uint16_t* px, const uint8_t* alpha, const uint8_t* cls, int16_t w, int16_t h, int16_t x,
                     int16_t y, const SpriteFx& fx) {
  int16_t dw = (int16_t)lroundf(w * fx.scaleX), dh = (int16_t)lroundf(h * fx.scaleY);
  if (dw <= 0 || dh <= 0 || dw > 320) return;
  // source column for each destination column, computed once (16.16 fixed point)
  int16_t cols[320];
  uint32_t stepX = ((uint32_t)w << 16) / dw, stepY = ((uint32_t)h << 16) / dh;
  for (int16_t i = 0; i < dw; i++) {
    int16_t sx = (int16_t)((i * stepX) >> 16);
    if (sx >= w) sx = w - 1;
    cols[i] = fx.flipX ? w - 1 - sx : sx;
  }
  for (int16_t j = 0; j < dh; j++) {
    int16_t ry = y + j + oy_;
    if (ry < cy0_ || ry >= cy1_) continue;
    int16_t sy = (int16_t)((j * stepY) >> 16);
    if (sy >= h) sy = h - 1;
    const size_t row = (size_t)sy * w;
    for (int16_t i = 0; i < dw; i++) {
      size_t idx = row + cols[i];
      uint8_t a = alpha[idx];
      if (!a) continue;
      uint16_t c = px[idx];
      uint8_t k = cls ? cls[idx] : 0;
      if (k == 1 && fx.skinTintOn) {
        uint32_t l = maxChannel(c) * 3u / 2u;  // keep the shading of the original green
        c = dim(fx.skinTint, (uint8_t)(l > 255 ? 255 : l));
      } else if (k == 2 && fx.eyeMix) {
        c = mix(c, fx.eyeColor, fx.eyeMix);
      } else if (k == 3 && fx.screenMix) {
        c = mix(c, fx.screenColor, fx.screenMix);
      }
      if (fx.bright != 255) c = dim(c, fx.bright);
      if (fx.flashMix) c = mix(c, fx.flash, fx.flashMix);
      pixel(x + i, y + j, c, fx.alpha == 255 ? a : (uint8_t)(a * fx.alpha / 255));
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
