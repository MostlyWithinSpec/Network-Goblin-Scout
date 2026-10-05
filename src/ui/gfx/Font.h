#pragma once
#include <stdint.h>

namespace gfx {

// Anti-aliased bitmap font: 4-bit coverage per pixel, two pixels per byte, row-major.
struct Glyph {
  uint32_t offset;  // into Font::bits
  uint8_t w, h;
  int8_t xoff, yoff;  // from the pen position (top of the line box)
  uint8_t adv;
};

struct Font {
  const uint8_t* bits;
  const Glyph* glyphs;
  uint8_t first, last;
  uint8_t lineH;
  uint8_t ascent;
};

}  // namespace gfx
