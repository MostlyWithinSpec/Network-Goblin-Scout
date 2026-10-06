#include "Loot.h"
#include <ctype.h>
#include <string.h>

namespace loot {

#include "LootData.inc"

const uint16_t kBrandCount = sizeof(kBrands) / sizeof(kBrands[0]);
static_assert(sizeof(kBrands) / sizeof(kBrands[0]) < 256, "brand index must fit in the table's low byte");
static_assert(sizeof(kBrands) / sizeof(kBrands[0]) <= 224, "widen Stats::kMaxBrands");

namespace {
// Pseudo-brands: the first entries of kBrands (tools/gen_oui.py)
const uint16_t B_MYSTERY = 0, B_ODD = 1, B_HOTSPOT = 2, B_DIRECT = 3, B_SMARTTV = 4;

const uint8_t kKindRarity[K_COUNT] = {
    R_COMMON,    // mystery
    R_COMMON,    // odd
    R_COMMON,    // ISP gateway
    R_COMMON,    // home router
    R_UNCOMMON,  // mesh
    R_UNCOMMON,  // business AP
    R_UNCOMMON,  // printer
    R_UNCOMMON,  // phone
    R_UNCOMMON,  // smart home
    R_UNCOMMON,  // TV
    R_RARE,      // console
    R_RARE,      // car
    R_RARE,      // camera / drone
    R_EPIC,      // satellite
    R_RARE,      // mobile router
    R_RARE,      // retail
    R_UNCOMMON,  // DIY
    R_RARE,      // industrial
};
const char* const kKindNames[K_COUNT] = {
    "Mystery boxes", "Odd boxes", "ISP gateways", "Home routers", "Mesh Wi-Fi", "Business APs",
    "Printers", "Phones", "Smart home", "TVs & streamers", "Game consoles", "Cars",
    "Cameras & drones", "Satellite", "Mobile routers", "Shops & tills", "Tinker boards", "Industrial",
};
const char* const kRarityNames[R_COUNT] = {"Common", "Uncommon", "Rare", "Epic", "Legendary"};
const uint32_t kXp[R_COUNT] = {1, 3, 8, 20, 50};

bool startsWithNoCase(const char* s, const char* prefix) {
  for (; *prefix; s++, prefix++)
    if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return false;
  return true;
}

bool containsNoCase(const char* s, const char* needle) {
  for (; *s; s++)
    if (startsWithNoCase(s, needle)) return true;
  return false;
}

// Brand from the network name, for addresses that don't say who made them.
uint16_t brandOfName(const char* ssid) {
  if (!ssid || !*ssid) return kNone;
  if (startsWithNoCase(ssid, "DIRECT-")) return B_DIRECT;
  if (startsWithNoCase(ssid, "[TV]")) return B_SMARTTV;
  static const char* const kHotspot[] = {"iPhone", "Galaxy", "Pixel", "AndroidAP", "OnePlus", "Redmi", "moto "};
  for (const char* h : kHotspot)
    if (containsNoCase(ssid, h)) return B_HOTSPOT;
  return kNone;
}
}  // namespace

uint16_t brandOfOui(uint32_t oui) {
  size_t lo = 0, hi = sizeof(kOui) / sizeof(kOui[0]);
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    uint32_t o = kOui[mid] >> 8;
    if (o == oui) return (uint16_t)(kOui[mid] & 0xFF);
    if (o < oui) lo = mid + 1;
    else hi = mid;
  }
  return kNone;
}

uint16_t brandByKey(const char* key) {
  for (uint16_t i = 0; i < kBrandCount; i++)
    if (strcmp(kBrands[i].key, key) == 0) return i;
  return kNone;
}

uint8_t kindRarity(uint8_t kind) { return kind < K_COUNT ? kKindRarity[kind] : (uint8_t)R_COMMON; }
const char* kindName(uint8_t kind) { return kind < K_COUNT ? kKindNames[kind] : "?"; }
const char* rarityName(uint8_t r) { return r < R_COUNT ? kRarityNames[r] : "?"; }
uint32_t xp(uint8_t r) { return r < R_COUNT ? kXp[r] : 1; }

Find classifyWifi(const uint8_t mac[6], const char* ssid, const WifiTraits& t) {
  uint16_t b;
  if (mac[0] & 0x02) {  // locally administered: a random or derived address, no maker code
    b = brandOfName(ssid);
    if (b == kNone) b = B_MYSTERY;
  } else {
    b = brandOfOui(((uint32_t)mac[0] << 16) | ((uint32_t)mac[1] << 8) | mac[2]);
    if (b == kNone) b = brandOfName(ssid);
    if (b == kNone) b = B_ODD;
  }
  // Starlink's own routers say so in the default name, whatever their address
  if (ssid && startsWithNoCase(ssid, "STARLINK")) b = brandByKey("starlink");
  Find f;
  f.brand = b;
  f.kind = kBrands[b].kind;
  uint8_t base = kBrands[b].rarity == R_KIND ? kindRarity(f.kind) : kBrands[b].rarity;
  uint8_t r = base;
  if (t.wep) r = R_LEGENDARY;                                  // a museum piece
  else if (t.open && f.kind == K_BIZ) r = R_LEGENDARY;         // an office network left wide open
  else if (t.enterprise && r < R_RARE) r = R_RARE;             // a login-protected corporate network
  else if (t.open && f.kind != K_MYSTERY && r < R_RARE) r++;   // somebody's router left open
  f.rarity = r;
  f.bonus = r != base;
  return f;
}

}  // namespace loot
