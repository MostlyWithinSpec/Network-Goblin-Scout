#!/bin/sh
# Render the goblin in every hat into tools/preview/out/goblins/*.png (transparent background)
# for the profile pages in the network-goblin-labs repo (scout/img/goblin/). Needs Pillow.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$ROOT/tools/preview/out/goblins"
mkdir -p "$OUT"
g++ -std=gnu++17 -O2 -Wall -Wextra -I "$ROOT/tools/preview/shim" -I "$ROOT/src" -I "$ROOT/include" \
  "$ROOT/tools/preview/goblins.cpp" "$ROOT/src/ui/Companion.cpp" "$ROOT/src/ui/HatArt.cpp" "$ROOT/src/core/Hats.cpp" \
  "$ROOT/src/ui/gfx/Surface.cpp" "$ROOT/src/core/Achievements.cpp" -o "$OUT/goblins"
cd "$OUT" && ./goblins
python3 - "$OUT" <<'PY'
import sys, glob, os
from PIL import Image
out = sys.argv[1]
for b in sorted(glob.glob(os.path.join(out, "*-b.ppm"))):
    w = b[:-6] + "-w.ppm"
    kb, kw = Image.open(b).convert("RGB"), Image.open(w).convert("RGB")
    res = Image.new("RGBA", kb.size)
    pb, pw, pr = kb.load(), kw.load(), res.load()
    for y in range(kb.size[1]):
        for x in range(kb.size[0]):
            cb, cw = pb[x, y], pw[x, y]
            # on black: c = a*fg; on white: c = a*fg + (1-a)*255  ->  a = 1 - (white - black)/255
            a = 255 - max(0, min(255, round(sum(cw[i] - cb[i] for i in range(3)) / 3)))
            pr[x, y] = (0, 0, 0, 0) if a == 0 else tuple(min(255, round(cb[i] * 255 / a)) for i in range(3)) + (a,)
    bbox = res.getbbox()
    if bbox:
        res = res.crop(bbox)
    res.save(b[:-6] + ".png", optimize=True)
    os.remove(b); os.remove(w)
print("goblins ->", out)
PY
