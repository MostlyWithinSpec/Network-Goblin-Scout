#pragma once
#include <stddef.h>
#include <stdint.h>

// Loot rarity: what kind of thing a new Wi-Fi network or Bluetooth gadget is, and how rare a find
// that makes it. Wi-Fi: the brand comes from the first half of the router's MAC address (the
// maker code, OUI) and, when that address is random, from a few name patterns. Bluetooth: from
// the company id in the advert (and what Apple devices say they are). Everything happens in RAM at the
// moment of the sighting: the address and name are never kept (design rule 1), only the
// brand key and the counts. Pure code: runs in the PC tests.
namespace loot {

// Kinds of loot. Append only (the Hoard Book shows them in this order).
enum Kind : uint8_t {
  K_MYSTERY, K_ODD, K_ISP, K_HOME, K_MESH, K_BIZ, K_PRINTER, K_PHONE, K_IOT, K_TV,
  K_CONSOLE, K_CAR, K_CAMERA, K_SAT, K_MOBILE, K_RETAIL, K_DIY, K_INDUSTRIAL,
  K_AUDIO, K_WEARABLE, K_TRACKER, K_HACKER,  // v0.6.1: Bluetooth loot and hacker gear
  K_COUNT
};

// Hacker gear the goblin reacts to (bit in Stats::hackerMask: append only).
enum Hacker : uint8_t { H_FLIPPER, H_PWNAGOTCHI, H_PINEAPPLE, H_DEAUTHER, H_BLESPAM, H_COUNT, H_NONE = 0xFF };

enum Rarity : uint8_t { R_COMMON, R_UNCOMMON, R_RARE, R_EPIC, R_LEGENDARY, R_COUNT, R_KIND = 0xFF };

struct Brand {
  const char* key;    // saved on SD: never change
  const char* name;   // shown on screen
  uint8_t kind;
  uint8_t rarity;     // R_KIND = the kind's usual rarity
};

extern const Brand kBrands[];
extern const uint16_t kBrandCount;
const uint16_t kNone = 0xFFFF;

struct Find {
  uint16_t brand;     // index into kBrands
  uint8_t kind;
  uint8_t rarity;
  bool bonus;         // rarity went up because of how the network is set up (open, WEP, ...)
};

struct WifiTraits {
  bool open = false;
  bool wep = false;
  bool enterprise = false;  // 802.1X login (offices, campuses)
};

// What a Bluetooth advert says about its sender (filled in by the scanner, nothing kept).
struct BleInfo {
  bool hasCompany = false;
  uint16_t company = 0;       // manufacturer data company id
  uint8_t msgType = 0;        // first byte after the company id (Apple: Continuity type), 0 = none
  uint8_t tracker = 0;        // trackers::Kind, 0 = not a tracker
  bool fastPair = false;      // Google Fast Pair service data (headphones and friends)
  bool flipperSvc = false;    // Flipper Zero's own service UUIDs 0x3081-0x3083
  bool iBeacon = false;
  bool squach = false;        // a SquachWatch's SquachMesh advert (company 0xFFFF, "SQM1")
  const char* name = nullptr; // advertised local name, may be null
};

uint16_t brandOfOui(uint32_t oui);              // 0xAABBCC -> brand index, or kNone
uint16_t brandOfCompany(uint16_t company);      // Bluetooth SIG company id -> brand index, or kNone
uint16_t brandByKey(const char* key);           // kNone if unknown (e.g. from a newer firmware)
Find classifyWifi(const uint8_t mac[6], const char* ssid, const WifiTraits& t);
Find classifyBle(const BleInfo& b);
// Hacker gear, checked on every sighting (not only new ones): the goblin reacts each time.
uint8_t hackerOfWifi(const uint8_t mac[6], const char* ssid);  // Hacker or H_NONE
uint8_t hackerOfBle(const BleInfo& b);
bool blePopup(const BleInfo& b);  // a "connect me" popup advert: lots of them at once = BLE spam
const char* hackerName(uint8_t h);

uint8_t kindRarity(uint8_t kind);
const char* kindName(uint8_t kind);
const char* rarityName(uint8_t rarity);
uint32_t xp(uint8_t rarity);                    // bonus XP for the first network with a new name

}  // namespace loot
