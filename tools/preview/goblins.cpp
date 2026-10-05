// Renders the goblin in every hat (and each hat on its own) with the real UI code, for the
// goblin profile pages on scout.networkgoblin.dev. Each picture is drawn twice, on black and on
// white, and tools/preview/goblins.sh turns the pair into a PNG with a transparent background.
#include <cstdio>
#include "core/Hats.h"
#include "ui/Companion.h"
#include "ui/HatArt.h"

static void save(const char* name, const uint16_t* px, int w, int h) {
  FILE* f = fopen(name, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int i = 0; i < w * h; i++) {
    uint16_t c = px[i];
    unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3 | (c >> 13)), (unsigned char)(((c >> 5) & 0x3F) << 2 | ((c >> 9) & 3)),
                            (unsigned char)((c & 0x1F) << 3 | ((c >> 2) & 7))};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

constexpr int GW = 240, GH = 300;  // goblin wearing a hat, scale 2
constexpr int HW = 96, HH = 96;    // hat on its own
static uint16_t gpx[GW * GH], hpx[HW * HH];

int main() {
  const uint32_t now = 1000;  // eyes open, mid-breath
  for (uint8_t id = 0; id <= hats::kCount; id++) {
    for (int pass = 0; pass < 2; pass++) {
      const uint16_t back = pass ? 0xFFFF : 0x0000;
      const char* tag = pass ? "w" : "b";
      char path[64];
      gfx::Surface g(gpx, GW, GH);
      g.clear(back);
      Companion::Look look;
      look.scale = 2;
      look.aura = false;
      look.hat = id;
      Companion::drawGoblin(g, GW / 2, GH - 6, now, CState::Idle, look);
      snprintf(path, sizeof(path), "goblin-%u-%s.ppm", id, tag);
      save(path, gpx, GW, GH);
      if (id == 0) continue;
      gfx::Surface h(hpx, HW, HH);
      h.clear(back);
      ui::drawHat(h, id, HW / 2, HH * 0.78f, 2.0f, now, false);
      snprintf(path, sizeof(path), "hat-%u-%s.ppm", id, tag);
      save(path, hpx, HW, HH);
    }
  }
  return 0;
}
