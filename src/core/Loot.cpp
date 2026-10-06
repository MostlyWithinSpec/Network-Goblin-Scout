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
const uint16_t B_MYSTERY = 0, B_ODD = 1, B_HOTSPOT = 2, B_DIRECT = 3, B_SMARTTV = 4, B_FASTPAIR = 5;
const uint16_t B_FLIPPER = 6, B_PWNAGOTCHI = 7, B_PINEAPPLE = 8, B_DEAUTHER = 9, B_SQUACH = 10;

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
    R_UNCOMMON,  // audio
    R_UNCOMMON,  // wearable
    R_UNCOMMON,  // tracker
    R_LEGENDARY, // hacker gear
};
// Short: the Hoard Book shows them in three columns.
const char* const kKindNames[K_COUNT] = {
    "Mystery", "Odd boxes", "ISP boxes", "Routers", "Mesh Wi-Fi", "Office APs",
    "Printers", "Phones", "Smart home", "TVs", "Gaming", "Cars",
    "Cameras", "Satellite", "Mobile Wi-Fi", "Shops", "Tinker kit", "Industrial",
    "Audio", "Wearables", "Trackers", "Hackers",
};
const char* const kHackerNames[H_COUNT] = {"Flipper Zero", "Pwnagotchi", "Wi-Fi Pineapple", "Deauther", "BLE spam"};
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

uint16_t brandOfCompany(uint16_t company) {
  size_t lo = 0, hi = sizeof(kCompany) / sizeof(kCompany[0]);
  while (lo < hi) {
    size_t mid = (lo + hi) / 2;
    uint32_t c = kCompany[mid] >> 8;
    if (c == company) return (uint16_t)(kCompany[mid] & 0xFF);
    if (c < company) lo = mid + 1;
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
const char* hackerName(uint8_t h) { return h < H_COUNT ? kHackerNames[h] : "?"; }
uint32_t xp(uint8_t r) { return r < R_COUNT ? kXp[r] : 1; }

// Hacker gear on Wi-Fi, from what it broadcasts by default:
//   Pwnagotchi: beacons from DE:AD:BE:EF:DE:AD (how pwnagotchis find each other)
//   Hak5 Wi-Fi Pineapple: management network "Pineapple_XXXX"
//   ESP8266/ESP32 deauther: control network "pwned"
uint8_t hackerOfWifi(const uint8_t mac[6], const char* ssid) {
  static const uint8_t kPwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
  if (memcmp(mac, kPwn, 6) == 0) return H_PWNAGOTCHI;
  if (ssid && startsWithNoCase(ssid, "Pineapple_")) return H_PINEAPPLE;
  if (ssid && strncmp(ssid, "pwned", 5) == 0) return H_DEAUTHER;
  return H_NONE;
}

// Flipper Zero: Flipper Devices' company id 0x0E29, its own service UUIDs 0x3081-0x3083, or the
// default "Flipper <name>" advert name.
uint8_t hackerOfBle(const BleInfo& b) {
  if ((b.hasCompany && b.company == 0x0E29) || b.flipperSvc) return H_FLIPPER;
  if (b.name && strncmp(b.name, "Flipper ", 8) == 0) return H_FLIPPER;
  return H_NONE;
}

// Popup adverts: Apple "Nearby Action" (0x0F) and Microsoft Swift Pair. Real devices send them
// now and then; spam tools send dozens from fresh addresses every second.
bool blePopup(const BleInfo& b) {
  return b.hasCompany && ((b.company == 0x004C && b.msgType == 0x0F) || (b.company == 0x0006 && b.msgType == 0x03));
}

Find classifyBle(const BleInfo& in) {
  Find f{};
  uint16_t b = kNone;
  uint8_t kind = 0xFF;
  if (hackerOfBle(in) == H_FLIPPER) b = B_FLIPPER;
  if (in.squach) b = B_SQUACH;
  if (b == kNone && in.hasCompany) b = brandOfCompany(in.company);
  if (in.tracker) kind = K_TRACKER;
  if (b == kNone && in.tracker) {
    // tracker services without a company id (trackers::Kind values: 1 Find My .. 5 Chipolo)
    static const char* const kTrk[] = {nullptr, "apple", "samsung", "tilebt", "google", "chipolo"};
    if (in.tracker < sizeof(kTrk) / sizeof(kTrk[0]) && kTrk[in.tracker]) b = brandByKey(kTrk[in.tracker]);
  }
  if (b == kNone && in.fastPair) b = B_FASTPAIR;
  if (b == kNone) b = in.hasCompany ? B_ODD : B_MYSTERY;
  if (kind == 0xFF && in.hasCompany && in.company == 0x004C) {
    // Apple Continuity: what the device says it is
    switch (in.msgType) {
      case 0x07: kind = K_AUDIO; break;            // AirPods / Beats pairing
      case 0x09: kind = K_TV; break;               // AirPlay target: Apple TV, HomePod
      case 0x12: kind = K_TRACKER; break;          // Find My
      case 0x02: kind = K_RETAIL; break;           // iBeacon
      default: kind = K_PHONE; break;              // nearby info, handoff, AirDrop...
    }
  }
  if (kind == 0xFF && in.iBeacon) kind = K_RETAIL;  // shop beacons
  if (kind == 0xFF) kind = kBrands[b].kind;
  f.brand = b;
  f.kind = kind;
  // a brand's own rarity applies to its usual kind; anything else uses the kind's rarity
  f.rarity = kBrands[b].rarity != R_KIND && kind == kBrands[b].kind ? kBrands[b].rarity : kindRarity(kind);
  f.bonus = false;
  return f;
}

Find classifyWifi(const uint8_t mac[6], const char* ssid, const WifiTraits& t) {
  uint8_t h = hackerOfWifi(mac, ssid);
  if (h != H_NONE) {
    static const uint16_t kHb[] = {B_FLIPPER, B_PWNAGOTCHI, B_PINEAPPLE, B_DEAUTHER};
    Find f{};
    f.brand = kHb[h];
    f.kind = K_HACKER;
    f.rarity = R_LEGENDARY;
    return f;
  }
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
