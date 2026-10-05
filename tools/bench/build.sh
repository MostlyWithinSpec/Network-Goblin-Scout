#!/bin/sh
# Cross-compile the UI benchmark for rv32imac (ESP32-C5 CPU: no FPU). Usage: sh tools/bench/build.sh [surface.cpp]
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
T=${TOOLCHAIN:-$HOME/.platformio/packages/toolchain-riscv32-esp/bin}
SURFACE=${1:-$ROOT/src/ui/gfx/Surface.cpp}
"$T/riscv32-esp-elf-g++" -march=rv32imac_zicsr_zifencei -mabi=ilp32 -Os -std=gnu++2b -fno-exceptions -fno-rtti \
  --specs=nosys.specs -I "$ROOT/tools/preview/shim" -I "$ROOT/src" -I "$ROOT/include" \
  "$ROOT/tools/bench/bench.cpp" "$ROOT/src/ui/Ui.cpp" "$ROOT/src/ui/Widgets.cpp" "$ROOT/src/ui/Companion.cpp" \
  "$SURFACE" "$ROOT/src/core/Achievements.cpp" -lm -o "${OUT:-$ROOT/tools/bench/bench.elf}"
