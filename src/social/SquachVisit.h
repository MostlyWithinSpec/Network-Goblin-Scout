#pragma once
#include <stddef.h>
#include <stdint.h>

// Recognises a SquachWatch (github.com/skizzophrenic/SquachWatch-CYD) saying hello over SquachMesh,
// so the goblin can greet a visiting Squachy. Our own reading of their published wire format
// (include/squachmesh.h there), not their code. It rides in BLE manufacturer data under company
// 0xFFFF (the same test id our goblin beacon uses), payload after the company id:
//   0-3  "SQM1"   4  version 1   5-6  appearance word, little-endian   7  flags, must be 0
//   8-19 name (ASCII, NUL padded), only when appearance bit 5 ("custom name") is set
// Appearance bit 3: "the Legend's aura is lit". Message frames (encrypted chat) are something else
// and are ignored. Pure code: tested on the PC (test/test_peer.cpp).
namespace squachvisit {

struct Visitor {
  char name[13] = "";  // typed by its owner, or "" for one of the stock nicknames
  bool aura = false;
};

inline bool decode(const uint8_t* d, size_t len, Visitor& out) {
  if (len != 8 && len != 20) return false;
  if (d[0] != 'S' || d[1] != 'Q' || d[2] != 'M' || d[3] != '1' || d[4] != 1 || d[7] != 0) return false;
  uint16_t word = (uint16_t)(d[5] | d[6] << 8);
  bool custom = word & (1u << 5);
  if (custom != (len == 20)) return false;
  Visitor v;
  v.aura = word & (1u << 3);
  if (custom) {
    size_t n = 0;
    while (n < 12 && d[8 + n]) {
      if (d[8 + n] < 0x20 || d[8 + n] > 0x7E) return false;
      v.name[n] = (char)d[8 + n];
      n++;
    }
    if (!n) return false;
    v.name[n] = 0;
  }
  out = v;
  return true;
}

}  // namespace squachvisit
