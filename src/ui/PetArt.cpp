#include "PetArt.h"
#include <math.h>

namespace ui {

using gfx::hex;

// Pip: a torico (tortie + calico). Black, with a white bib, chin and paws, ginger patches on her
// body, and a ginger patch over one side of her face down to a pink nose. Green-gold eyes.
const CatCoat kPip = {"Pip", hex(0x141416), hex(0xD99A6C), hex(0xB8784E), hex(0xB8C25A), 1.0f,
                      true, true, false, true, false, 6, false};
// Lily: the tiny tortie. Brindled black-brown and ginger all over, a ginger blaze down her forehead
// and nose, big amber-green kitten eyes, one white-tipped paw, and plenty of fluff.
const CatCoat kLily = {"Lily", hex(0x2A211C), hex(0xB07C55), hex(0x6E4A34), hex(0xB0A24A), 0.8f,
                       false, false, true, false, true, 54, true};

namespace {
const uint16_t kWhite = hex(0xF2EFEA), kInk = hex(0x0A0A0C), kPink = hex(0xE8A0A8), kWhisker = hex(0xD8D8D8);

struct Pen {  // coordinates in "cat units" around (x, y), scaled by k
  gfx::Surface& s;
  float x, y, k;
  uint8_t a;
  void circle(float cx, float cy, float r, uint16_t c) const { s.fillCircle(x + cx * k, y + cy * k, r * k, c, a); }
  void tri(float x0, float y0, float x1, float y1, float x2, float y2, uint16_t c) const {
    s.fillTriangle((int16_t)(x + x0 * k), (int16_t)(y + y0 * k), (int16_t)(x + x1 * k), (int16_t)(y + y1 * k),
                   (int16_t)(x + x2 * k), (int16_t)(y + y2 * k), c, a);
  }
  void line(float x0, float y0, float x1, float y1, uint16_t c, uint8_t al) const {
    s.line((int16_t)(x + x0 * k), (int16_t)(y + y0 * k), (int16_t)(x + x1 * k), (int16_t)(y + y1 * k), c, al);
  }
  void rrect(float rx, float ry, float w, float h, float r, uint16_t c) const {
    s.fillRoundRect((int16_t)(x + rx * k), (int16_t)(y + ry * k), (int16_t)(w * k + 0.5f), (int16_t)(h * k + 0.5f),
                    (int16_t)(r * k), c, a);
  }
};

// Tortie speckles: the same pattern every frame (fixed seed), inside a circle.
void speckles(const Pen& p, const CatCoat& c, float cx, float cy, float r, int n, uint32_t seed) {
  uint32_t v = seed;
  for (int i = 0; i < n; i++) {
    v = v * 1103515245u + 12345u;
    float ang = (v >> 8 & 1023) * (6.2831853f / 1024);
    float d = r * ((v >> 18 & 255) / 255.0f) * 0.9f;
    float sz = 0.7f + (v >> 26 & 3) * 0.3f;
    p.circle(cx + cosf(ang) * d, cy + sinf(ang) * d, sz, (i & 1) ? c.patch2 : c.patch);
  }
}

bool blinking(uint32_t now, uint32_t seed) {
  uint32_t t = (now + seed * 1337u) % 4700u;
  return t < 150 || (t > 300 && t < 420 && (seed & 1));  // sometimes a double blink
}

void eye(const Pen& p, const CatCoat& c, float ex, float ey, float r, bool closed, bool wide, float look) {
  if (closed) {
    p.line(ex - r, ey, ex + r, ey + 0.3f, kInk, 230);
    return;
  }
  p.circle(ex, ey, r, c.eyes);
  if (wide) p.circle(ex + look, ey, r * 0.55f, kInk);  // round pupils: something interesting
  else p.rrect(ex - 0.55f + look * 0.5f, ey - r * 0.75f, 1.1f, r * 1.5f, 0.5f, kInk);
  p.circle(ex - r * 0.35f, ey - r * 0.4f, r * 0.22f, kWhite);
}

void sitting(gfx::Surface& s, const CatCoat& c, float x, float y, float k, uint32_t now, PetPose pose, uint32_t seed) {
  Pen p{s, x, y, k, 255};
  float t = now / 1000.0f + seed * 0.37f;
  // tail, behind everything: curls up beside the body and swishes
  float swish = sinf(t * 1.7f) * (pose == PetPose::Alert ? 6.0f : 3.0f);
  for (int i = 0; i <= 8; i++) {
    float f = i / 8.0f;
    float tx = 8 + 11 * sinf(f * 1.7f) + swish * f * f, ty = -2 - 17 * f + 5 * f * f;
    p.circle(tx, ty, 2.7f - f * 0.6f, (c.brindle > 10 && (i % 3 == 1)) ? c.patch2 : c.base);
  }
  // body and front legs
  p.circle(0, -11, 11.5f, c.base);
  p.circle(0, -19, 8.5f, c.base);
  p.rrect(-7, -16, 5.5f, 15, 2.5f, c.base);
  p.rrect(1.5f, -16, 5.5f, 15, 2.5f, c.base);
  if (c.brindle) speckles(p, c, 0, -12, 10, c.brindle * 2 / 3, 0xC0A7u + seed);
  if (c.bib) {  // Pip's white chest, and her ginger body patches
    p.circle(0, -17.5f, 5.5f, kWhite);
    p.tri(-4.5f, -15, 4.5f, -15, 0, -8, kWhite);
    p.circle(-8, -9, 2.4f, c.patch2);
    p.circle(-7, -6.5f, 1.6f, c.patch);
    p.circle(8, -12, 1.9f, c.patch2);
  }
  // paws
  p.circle(-4.2f, -1.4f, 3.2f, c.whitePaws || c.whiteTip ? kWhite : c.base);
  p.circle(4.2f, -1.4f, 3.2f, c.whitePaws ? kWhite : c.base);
  p.line(-4.2f, -2.5f, -4.2f, 0, kInk, 90);
  p.line(4.2f, -2.5f, 4.2f, 0, kInk, 90);
  // head
  float earUp = pose == PetPose::Alert ? 2.5f : 0;
  bool flick = ((now + seed * 911u) % 6100u) < 140;  // an ear twitch now and then
  p.tri(-10, -33, -3, -39, -9, -47 - earUp + (flick ? 2 : 0), c.base);
  p.tri(10, -33, 3, -39, 9, -47 - earUp, c.base);
  p.tri(-8.4f, -36, -5, -39, -8.2f, -43.5f - earUp + (flick ? 2 : 0), kPink);
  p.tri(8.4f, -36, 5, -39, 8.2f, -43.5f - earUp, kPink);
  p.circle(0, -31, 10.5f, c.base);
  p.circle(-5, -28.5f, 6.5f, c.base);
  p.circle(5, -28.5f, 6.5f, c.base);
  if (c.fluffy) {  // kitten fluff
    p.tri(-10.5f, -30, -14, -26.5f, -9, -25, c.base);
    p.tri(10.5f, -30, 14, -26.5f, 9, -25, c.base);
    p.tri(-2, -41, 0, -44, 2, -41, c.base);
  }
  if (c.brindle) speckles(p, c, 0, -32, 8, c.brindle / 3, 0x5EEDu + seed);
  if (c.halfFace) {  // Pip: ginger over one side of the face, down to the nose
    p.circle(4.8f, -28.2f, 3.6f, c.patch);
    p.circle(2.2f, -27.6f, 2.2f, c.patch);
    p.circle(7, -31, 2, c.patch2);
  }
  if (c.blaze) {  // Lily: the ginger stripe from forehead to nose
    p.circle(0, -38.5f, 2.6f, c.patch);
    p.circle(0, -35, 2.1f, c.patch);
    p.circle(0, -31.5f, 1.6f, c.patch);
    p.circle(0, -28.8f, 1.3f, c.patch);
  }
  if (c.bib) p.circle(0, -25.3f, 3.6f, kWhite);  // white chin
  // face
  bool closed = pose == PetPose::Sleep || blinking(now, seed);
  bool wide = pose == PetPose::Alert || pose == PetPose::Hop;
  float look = pose == PetPose::Alert ? sinf(t * 1.3f) * 0.8f : 0;
  float er = c.size < 0.9f ? 2.9f : 2.4f;  // kittens: big eyes
  eye(p, c, -4.2f, -32.5f, er, closed, wide, look);
  eye(p, c, 4.2f, -32.5f, er, closed, wide, look);
  p.tri(-1.5f, -28.6f, 1.5f, -28.6f, 0, -27, c.halfFace ? kPink : hex(0x3A2A22));
  p.line(0, -27, -1.5f, -25.8f, kInk, 150);
  p.line(0, -27, 1.5f, -25.8f, kInk, 150);
  for (int i = -1; i <= 1; i += 2) {
    p.line(i * 4.5f, -27.5f, i * 14, -29.5f, kWhisker, 120);
    p.line(i * 4.5f, -26.8f, i * 14, -26.5f, kWhisker, 120);
  }
}

void curled(gfx::Surface& s, const CatCoat& c, float x, float y, float k, uint32_t now, uint32_t seed) {
  Pen p{s, x, y, k, 255};
  float breath = sinf(now / 900.0f + seed) * 0.6f;
  p.circle(-1, -7.5f - breath, 9.5f, c.base);
  p.circle(7, -6.5f - breath * 0.7f, 8, c.base);
  p.circle(-8, -5, 6.5f, c.base);
  if (c.brindle) speckles(p, c, 1, -7, 8.5f, c.brindle / 2, 0xC0A7u + seed);
  if (c.bib) {
    p.circle(5, -10 - breath, 2.2f, c.patch);
    p.circle(-1, -13 - breath, 1.7f, c.patch2);
  }
  for (int i = 0; i <= 8; i++) {  // tail wrapped round the front
    float f = i / 8.0f;
    p.circle(13 - 25 * f, -1.8f - sinf(f * 3.1416f) * 1.5f, 2.7f - f * 0.5f,
             (c.brindle > 10 && (i % 3 == 1)) ? c.patch2 : c.base);
  }
  // head tucked in on the left, nose on the paws
  p.tri(-15, -11, -12, -14, -16, -19, c.base);
  p.tri(-6, -14, -3, -13, -5, -20, c.base);
  p.circle(-9.5f, -9.5f, 7, c.base);
  if (c.halfFace) p.circle(-7, -8.5f, 2.3f, c.patch);
  if (c.blaze) {
    p.circle(-9.5f, -14, 1.8f, c.patch);
    p.circle(-9.5f, -10.5f, 1.4f, c.patch);
  }
  if (c.bib) p.circle(-9.5f, -4.5f, 2.6f, kWhite);
  if (c.whitePaws || c.whiteTip) p.circle(-13, -2.5f, 2.4f, kWhite);
  p.line(-13, -10, -11, -9.4f, kInk, 230);  // closed eyes
  p.line(-8, -9.4f, -6, -10, kInk, 230);
}
}  // namespace

void drawCat(gfx::Surface& s, const CatCoat& c, float x, float y, float k, uint32_t now, PetPose pose,
             uint32_t seed) {
  k *= c.size;
  s.fillRoundRect((int16_t)(x - 12 * k), (int16_t)(y - 1.5f * k), (int16_t)(24 * k), (int16_t)(4 * k),
                  (int16_t)(2 * k), 0x0000, 80);  // shadow
  if (pose == PetPose::Sleep) {
    curled(s, c, x, y, k, now, seed);
    return;
  }
  if (pose == PetPose::Hop) y -= fabsf(sinf(now / 140.0f + seed)) * 7 * k;
  sitting(s, c, x, y, k, now, pose, seed);
}

}  // namespace ui
