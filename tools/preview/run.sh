#!/bin/sh
# Build the UI for the PC and render screenshots into tools/preview/out/.
#   sh tools/preview/run.sh            (needs g++ and python3 with Pillow)
#   PREVIEW_FLAGS="-g -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=all" \
#     sh tools/preview/run.sh          (memory checker: stops at the first overflow; CI runs this)
set -e
FLAGS=${PREVIEW_FLAGS:-}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$ROOT/tools/preview/out"
mkdir -p "$OUT"
gcc -std=c99 -O2 $FLAGS -c "$ROOT/lib/qrcodegen/qrcodegen.c" -o "$OUT/qrcodegen.o"
g++ -std=gnu++17 -O2 $FLAGS -Wall -Wextra -I "$ROOT/tools/preview/shim" -I "$ROOT/src" -I "$ROOT/include" -I "$ROOT/lib/qrcodegen" \
  "$ROOT/tools/preview/preview.cpp" "$ROOT/src/ui/Ui.cpp" "$ROOT/src/ui/Widgets.cpp" "$ROOT/src/ui/Companion.cpp" "$ROOT/src/ui/HatArt.cpp" "$ROOT/src/core/Quests.cpp" "$ROOT/src/core/Hats.cpp" "$ROOT/src/core/Trackers.cpp" "$ROOT/src/core/Loot.cpp" "$OUT/qrcodegen.o" \
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
