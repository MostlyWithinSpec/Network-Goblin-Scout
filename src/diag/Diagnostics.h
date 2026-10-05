#pragma once
#include "../scanners/Scanner.h"

// Hardware bring-up mode: one screen (mirrored to serial every 2 s) showing what
// each peripheral is doing, so a single flash tells us what works on a new board.
// It drives the scanners directly with a counting sink: no Engine, no XP, nothing
// written to the user's progress files, and only counts are shown (no MACs/SSIDs).
namespace diag {
// Waits up to windowMs for the BOOT button. True if it was pressed (or already held).
bool requested(uint32_t windowMs);
// Runs the diagnostics screen until reset. Never returns.
[[noreturn]] void run(Scanner* const* scanners, size_t count);
}  // namespace diag
