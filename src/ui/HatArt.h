#pragma once
#include <stdint.h>
#include "gfx/Surface.h"

// Draws hat `id` (core/Hats.h) on a goblin. (cx, brimY) = centre of the head where the
// hat sits, `sc` = goblin scale (1 = the 107 px sprite). Vector art, so it scales cleanly.
namespace ui {
void drawHat(gfx::Surface& s, uint8_t id, float cx, float brimY, float sc, uint32_t now, bool flip);
}
