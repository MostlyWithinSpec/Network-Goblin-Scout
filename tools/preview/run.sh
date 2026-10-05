#!/bin/sh
# Build the UI for the PC and render screenshots into tools/preview/out/.
#   sh tools/preview/run.sh            (needs g++ and python3 with Pillow)
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$ROOT/tools/preview/out"
mkdir -p "$OUT"
g++ -std=gnu++17 -O2 -Wall -Wextra -I "$ROOT/tools/preview/shim" -I "$ROOT/src" -I "$ROOT/include" \
  "$ROOT/tools/preview/preview.cpp" "$ROOT/src/ui/Ui.cpp" "$ROOT/src/ui/Widgets.cpp" "$ROOT/src/ui/Companion.cpp" \
  "$ROOT/src/ui/gfx/Surface.cpp" "$ROOT/src/core/Achievements.cpp" -o "$OUT/preview"
cd "$OUT" && ./preview
python3 - "$OUT" <<'PY'
import sys, glob, os
from PIL import Image
out = sys.argv[1]
files = sorted(glob.glob(os.path.join(out, "*.ppm")))
ims = []
for f in files:
    im = Image.open(f)
    im.save(f[:-4] + ".png")
    os.remove(f)
    ims.append(im)
cols = 4
rows = (len(ims) + cols - 1) // cols
sheet = Image.new("RGB", (cols * 330 + 10, rows * 250 + 10), (40, 40, 40))
for i, im in enumerate(ims):
    sheet.paste(im, (10 + (i % cols) * 330, 10 + (i // cols) * 250))
sheet.save(os.path.join(out, "contact_sheet.png"))
print(f"{len(ims)} screens -> {out}")
PY
