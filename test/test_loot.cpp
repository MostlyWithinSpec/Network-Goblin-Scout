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

  // pseudo-brands sit at fixed indexes (Loot.cpp relies on it)
  const char* pseudo[] = {"mystery", "odd", "hotspot", "direct", "smarttv", "fastpair", "flipper", "pwnagotchi",
                          "pineapple", "deauther", "squachwatch"};
  for (uint16_t i = 0; i < 11; i++) CHECK(strcmp(kBrands[i].key, pseudo[i]) == 0);

  // Bluetooth
  BleInfo bi;
  bi.hasCompany = true;
  bi.company = 0x004C;  // Apple
  bi.msgType = 0x07;    // proximity pairing: AirPods
  f = classifyBle(bi);
  CHECK(strcmp(key(f), "apple") == 0 && f.kind == K_AUDIO && f.rarity == R_UNCOMMON);
  bi.msgType = 0x10;  // nearby info: a phone
  f = classifyBle(bi);
  CHECK(f.kind == K_PHONE && f.rarity == R_RARE);  // Apple's own rarity applies to its usual kind
  bi.msgType = 0x09;
  CHECK(classifyBle(bi).kind == K_TV);
  bi = BleInfo{};
  bi.hasCompany = true;
  bi.company = 0x0087;  // Garmin
  f = classifyBle(bi);
  CHECK(strcmp(key(f), "garmin") == 0 && f.kind == K_WEARABLE);
  bi.company = 0x022B;  // Tesla
  f = classifyBle(bi);
  CHECK(strcmp(key(f), "tesla") == 0 && f.rarity == R_EPIC);
  bi.company = 0x7FFF;  // nobody we list
  CHECK(strcmp(key(classifyBle(bi)), "odd") == 0);
  bi = BleInfo{};
  CHECK(strcmp(key(classifyBle(bi)), "mystery") == 0);
  bi.tracker = 3;  // Tile service, no company id
  f = classifyBle(bi);
  CHECK(strcmp(key(f), "tilebt") == 0 && f.kind == K_TRACKER);
  bi = BleInfo{};
  bi.fastPair = true;
  CHECK(strcmp(key(classifyBle(bi)), "fastpair") == 0);

  // hacker gear
  bi = BleInfo{};
  bi.hasCompany = true;
  bi.company = 0x0E29;
  CHECK(hackerOfBle(bi) == H_FLIPPER);
  f = classifyBle(bi);
  CHECK(strcmp(key(f), "flipper") == 0 && f.kind == K_HACKER && f.rarity == R_LEGENDARY);
  bi = BleInfo{};
  bi.name = "Flipper Gobbo";
  CHECK(hackerOfBle(bi) == H_FLIPPER);
  bi.name = "Flippers R Us";
  CHECK(hackerOfBle(bi) == H_NONE);
  bi = BleInfo{};
  bi.flipperSvc = true;
  CHECK(hackerOfBle(bi) == H_FLIPPER);
  bi = BleInfo{};
  bi.hasCompany = true;
  bi.company = 0x004C;
  bi.msgType = 0x0F;
  CHECK(blePopup(bi));
  bi.msgType = 0x10;
  CHECK(!blePopup(bi));
  bi = BleInfo{};
  bi.hasCompany = true;
  bi.company = 0xFFFF;
  bi.squach = true;
  f = classifyBle(bi);
  CHECK(strcmp(key(f), "squachwatch") == 0 && f.kind == K_HACKER && f.rarity == R_LEGENDARY);
  const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
  CHECK(hackerOfWifi(pwn, "") == H_PWNAGOTCHI);
  f = classifyWifi(pwn, "", WifiTraits{});
  CHECK(strcmp(key(f), "pwnagotchi") == 0 && f.rarity == R_LEGENDARY);
  const uint8_t any[6] = {0x9C, 0xC9, 0xEB, 1, 2, 3};
  CHECK(hackerOfWifi(any, "Pineapple_1A2B") == H_PINEAPPLE);
  CHECK(hackerOfWifi(any, "pwned") == H_DEAUTHER);
  CHECK(hackerOfWifi(any, "PineappleExpress") == H_NONE);
  CHECK(hackerOfWifi(any, "Pwned by kids") == H_NONE);  // the deauther's name is lower case
  CHECK(strcmp(key(classifyWifi(any, "Pineapple_1A2B", WifiTraits{})), "pineapple") == 0);

  CHECK(xp(R_COMMON) == 1 && xp(R_LEGENDARY) == 50);
  CHECK(strcmp(rarityName(R_EPIC), "Epic") == 0);
  printf(fails ? "%d FAILED\n" : "loot: all tests passed\n", fails);
  return fails ? 1 : 0;
}
