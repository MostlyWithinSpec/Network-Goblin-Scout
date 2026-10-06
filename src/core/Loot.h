#pragma once
#include <stddef.h>
#include <stdint.h>

// Loot rarity: what kind of thing a new Wi-Fi network is, and how rare a find that makes it.
// The brand comes from the first half of the router's MAC address (the maker code, OUI) and,
// when that address is random, from a few name patterns. Everything happens in RAM at the
// moment of the sighting: the address and name are never kept (design rule 1), only the
// brand key and the counts. Pure code: runs in the PC tests.
namespace loot {

// Kinds of loot. Append only (the Hoard Book shows them in this order).
enum Kind : uint8_t {
  K_MYSTERY, K_ODD, K_ISP, K_HOME, K_MESH, K_BIZ, K_PRINTER, K_PHONE, K_IOT, K_TV,
  K_CONSOLE, K_CAR, K_CAMERA, K_SAT, K_MOBILE, K_RETAIL, K_DIY, K_INDUSTRIAL, K_COUNT
};

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

uint16_t brandOfOui(uint32_t oui);              // 0xAABBCC -> brand index, or kNone
uint16_t brandByKey(const char* key);           // kNone if unknown (e.g. from a newer firmware)
Find classifyWifi(const uint8_t mac[6], const char* ssid, const WifiTraits& t);

uint8_t kindRarity(uint8_t kind);
const char* kindName(uint8_t kind);
const char* rarityName(uint8_t rarity);
uint32_t xp(uint8_t rarity);                    // bonus XP for the first network with a new name

}  // namespace loot
