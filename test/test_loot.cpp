// PC test for core/Loot: g++ -std=c++17 -Wall -Wextra -I src test/test_loot.cpp src/core/Loot.cpp -o /tmp/t && /tmp/t
#include <cstdio>
#include <cstring>
#include "core/Loot.h"

static int fails = 0;
#define CHECK(c)                                          \
  do {                                                    \
    if (!(c)) {                                           \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
      fails++;                                            \
    }                                                     \
  } while (0)

using namespace loot;

static Find wifi(uint32_t oui, const char* ssid, WifiTraits t = {}) {
  uint8_t mac[6] = {(uint8_t)(oui >> 16), (uint8_t)(oui >> 8), (uint8_t)oui, 0x12, 0x34, 0x56};
  return classifyWifi(mac, ssid, t);
}
static const char* key(const Find& f) { return kBrands[f.brand].key; }

int main() {
  // table is sorted and every brand key is unique
  for (uint16_t i = 0; i < kBrandCount; i++) {
    CHECK(brandByKey(kBrands[i].key) == i);
    CHECK(kBrands[i].kind < K_COUNT);
    CHECK(strlen(kBrands[i].name) <= 14);
  }
  CHECK(brandByKey("nope") == kNone);

  // OUIs from the IEEE registry
  Find f = wifi(0x9CC9EB, "MyNet");  // NETGEAR
  CHECK(strcmp(key(f), "netgear") == 0 && f.rarity == R_COMMON && !f.bonus);
  f = wifi(0x00180A, "Corp");  // Cisco Meraki
  CHECK(strcmp(key(f), "meraki") == 0 && f.kind == K_BIZ && f.rarity == R_UNCOMMON);
  f = wifi(0x00180A, "Guest", {true, false, false});
  CHECK(f.rarity == R_LEGENDARY && f.bonus);  // open office network
  f = wifi(0x00180A, "Corp", {false, false, true});
  CHECK(f.rarity == R_RARE && f.bonus);  // enterprise login
  f = wifi(0x9CC9EB, "Old", {false, true, false});
  CHECK(f.rarity == R_LEGENDARY);  // WEP
  f = wifi(0x9CC9EB, "Open", {true, false, false});
  CHECK(f.rarity == R_UNCOMMON && f.bonus);  // open home router: one step up
  f = wifi(0x4CFCAA, "Tesla");  // Tesla,Inc.
  CHECK(strcmp(key(f), "tesla") == 0 && f.kind == K_CAR && f.rarity == R_EPIC);
  f = wifi(0x24A43C, "x");  // Ubiquiti
  CHECK(strcmp(key(f), "ubiquiti") == 0);

  // random addresses: name patterns, else a mystery box
  f = wifi(0x02AABB, "Bob's iPhone");
  CHECK(strcmp(key(f), "hotspot") == 0 && f.kind == K_PHONE);
  f = wifi(0x02AABB, "DIRECT-7A-HP OfficeJet");
  CHECK(strcmp(key(f), "direct") == 0);
  f = wifi(0x02AABB, "Home");
  CHECK(strcmp(key(f), "mystery") == 0 && f.rarity == R_COMMON);
  f = wifi(0x02AABB, "Home", {true, false, false});
  CHECK(f.rarity == R_COMMON && !f.bonus);  // open mystery boxes stay common (most are guest SSIDs)
  f = wifi(0x02AABB, "STARLINK");
  CHECK(strcmp(key(f), "starlink") == 0 && f.rarity == R_EPIC);
  f = wifi(0x00003F, "");  // a registered maker we don't list
  CHECK(strcmp(key(f), "odd") == 0);
  f = wifi(0x02AABB, nullptr);
  CHECK(strcmp(key(f), "mystery") == 0);

  CHECK(xp(R_COMMON) == 1 && xp(R_LEGENDARY) == 50);
  CHECK(strcmp(rarityName(R_EPIC), "Epic") == 0);
  printf(fails ? "%d FAILED\n" : "loot: all tests passed\n", fails);
  return fails ? 1 : 0;
}
