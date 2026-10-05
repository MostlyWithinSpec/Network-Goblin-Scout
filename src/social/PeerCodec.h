#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Wire format of the "I'm a goblin" BLE beacon. Pure C++ (no Arduino) so it can be
// unit-tested on a PC (test/test_peer.cpp). Carried as BLE manufacturer data:
//
//   0-1  0xFF 0xFF  company id 0xFFFF (Bluetooth SIG: "for testing / internal use")
//   2-3  'N' 'G'    magic
//   4    version (1)
//   5-8  goblin id (random per device, little-endian)
//   9-10 level (little-endian)
//   11   hue (colour of this goblin, 0-255)
//   12   flags: bits 0-4 = equipped hat id (0 = none, see core/Hats.h), 5-7 reserved
//   13+  goblin name, up to 12 chars, not NUL-terminated
//
// 13 + 12 = 25 bytes; with the AD headers and flags the advert is 30 of 31 bytes.
namespace peercodec {

const uint8_t kVersion = 1;
const size_t kHeader = 13;
const size_t kMaxName = 12;
const size_t kMaxLen = kHeader + kMaxName;

struct Info {
  uint32_t id = 0;
  uint16_t level = 0;
  uint8_t hue = 0;
  uint8_t flags = 0;
  char name[kMaxName + 1] = {0};
};

inline size_t encode(const Info& in, uint8_t* out) {
  out[0] = 0xFF; out[1] = 0xFF; out[2] = 'N'; out[3] = 'G'; out[4] = kVersion;
  for (int i = 0; i < 4; i++) out[5 + i] = (uint8_t)(in.id >> (8 * i));
  out[9] = (uint8_t)in.level; out[10] = (uint8_t)(in.level >> 8);
  out[11] = in.hue; out[12] = in.flags;
  size_t n = strnlen(in.name, kMaxName);
  memcpy(out + kHeader, in.name, n);
  return kHeader + n;
}

// Accepts newer versions too (fields are only ever appended).
inline bool decode(const uint8_t* d, size_t len, Info& out) {
  if (len < kHeader || d[0] != 0xFF || d[1] != 0xFF || d[2] != 'N' || d[3] != 'G' || d[4] < 1) return false;
  out.id = (uint32_t)d[5] | ((uint32_t)d[6] << 8) | ((uint32_t)d[7] << 16) | ((uint32_t)d[8] << 24);
  out.level = (uint16_t)(d[9] | (d[10] << 8));
  out.hue = d[11];
  out.flags = d[12];
  size_t n = len - kHeader;
  if (n > kMaxName) n = kMaxName;
  for (size_t i = 0; i < n; i++) {  // printable ASCII only; it's shown on screen
    char c = (char)d[kHeader + i];
    out.name[i] = (c >= 32 && c < 127) ? c : '?';
  }
  out.name[n] = 0;
  return out.id != 0;
}

// Deterministic goblin name from the id, e.g. "Snagpacket".
inline void nameFor(uint32_t id, char* out, size_t outLen) {
  static const char* const kPre[] = {"Grub", "Snag", "Zib",  "Wort", "Nix",  "Grim", "Fizz", "Mug",
                                     "Skrit", "Bog", "Rot",  "Glim", "Twig", "Zork", "Kip",  "Dreg"};
  static const char* const kSuf[] = {"nik",  "gle",  "bit",  "wick", "zle",  "snout", "tooth", "byte",
                                     "ping", "fang", "scrap", "ear", "sock", "nose", "ling",  "hex"};
  const char* a = kPre[id & 0x0F];
  const char* b = kSuf[(id >> 4) & 0x0F];
  size_t i = 0;
  for (const char* p = a; *p && i + 1 < outLen; p++) out[i++] = *p;
  for (const char* p = b; *p && i + 1 < outLen; p++) out[i++] = *p;
  out[i] = 0;
}

}  // namespace peercodec
