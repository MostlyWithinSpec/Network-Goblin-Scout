#include "Quests.h"
#include <stdio.h>

namespace quests {
namespace {
struct Def {
  const char* fmt;     // %u = target
  const char* fmt1;    // when target == 1
  uint16_t lo, hi;     // target range at level 1 (grows a little with level)
  uint16_t xpEach;     // reward per unit
  uint16_t xpBase;
};

// Index = Type.
const Def kDefs[kTypeCount] = {
    {"Sniff out %u new networks", "Sniff out a new network", 10, 30, 3, 20},
    {"Find %u new 5 GHz networks", "Find a new 5 GHz network", 3, 8, 8, 20},
    {"Spot %u new Bluetooth devices", "Spot a new Bluetooth device", 5, 15, 4, 20},
    {"Overhear %u new mesh devices", "Overhear a new mesh device", 1, 4, 20, 30},
    {"Hear %u new Wi-Fi channels", "Hear a new Wi-Fi channel", 1, 2, 40, 30},
    {"Meet %u goblins", "Meet another goblin", 1, 1, 0, 150},
    {"Pet your goblin %u times", "Pet your goblin", 5, 12, 3, 10},
    {"Run %u scans", "Run a scan", 40, 90, 1, 20},
    {"Detect %u hidden networks", "Detect a hidden network", 1, 3, 25, 20},
    {"Find %u WPA3 networks", "Find a WPA3 network", 2, 5, 15, 20},
    {"Explore %u new areas", "Explore a new area", 1, 2, 60, 40},
    {"Find %u Wi-Fi 6 routers", "Find a Wi-Fi 6 router", 2, 6, 12, 20},
    {"Find %u enterprise networks", "Find an enterprise network", 1, 3, 25, 20},
    {"Find %u open networks", "Find an open network", 1, 4, 15, 20},
};

bool available(const Stats& s, uint8_t t) {
  switch (t) {
    case kNewMesh: return s.t154Unique > 0;    // impossible in homes with no mesh gear
    case kMeetGoblin: return s.peersMet > 0;   // only once they know other goblins exist
    case kNewArea: return s.geoCells > 0;      // needs a GPS
    case kNewChannel: return s.channels.count() < 30;
    case kEnterprise: return s.wifiEnterprise > 0;
    default: return true;
  }
}

uint32_t lcg(uint32_t& x) {
  x = x * 1664525u + 1013904223u;
  return x >> 8;
}
}  // namespace

uint32_t metric(const Stats& s, uint8_t type) {
  switch (type) {
    case kNewWifi: return s.wifiUnique;
    case kNew5G: return s.wifi5g;
    case kNewBle: return s.bleUnique;
    case kNewMesh: return s.t154Unique;
    case kNewChannel: return (uint32_t)s.channels.count();
    case kMeetGoblin: return s.peerEncounters;
    case kPetGoblin: return s.pets;
    case kScans: return s.scans;
    case kHidden: return s.wifiHidden;
    case kWpa3: return s.wifiWpa3;
    case kNewArea: return s.geoCells;
    case kWifi6: return s.wifi6;
    case kEnterprise: return s.wifiEnterprise;
    case kOpen: return s.wifiOpen;
    default: return 0;
  }
}

uint32_t progress(const Stats& s, const Quest& q) {
  uint32_t m = metric(s, q.type);
  uint32_t p = m > q.base ? m - q.base : 0;
  return p > q.target ? q.target : p;
}

uint32_t reward(const Quest& q) {
  if (q.type >= kTypeCount) return 0;
  return kDefs[q.type].xpBase + (uint32_t)kDefs[q.type].xpEach * q.target;
}

void describe(const Quest& q, char* out, size_t n) {
  if (q.type >= kTypeCount) { snprintf(out, n, "?"); return; }
  const Def& d = kDefs[q.type];
  if (q.target == 1) snprintf(out, n, "%s", d.fmt1);
  else snprintf(out, n, d.fmt, q.target);
}

void deal(const Stats& s, uint16_t level, uint32_t seed, QuestBoard& board) {
  uint8_t pool[kTypeCount];
  size_t n = 0;
  for (uint8_t t = 0; t < kTypeCount; t++)
    if (available(s, t)) pool[n++] = t;
  uint32_t x = seed ? seed : 1;
  for (int i = 0; i < 3; i++) {
    size_t k = lcg(x) % n;  // pick without repeats
    uint8_t t = pool[k];
    pool[k] = pool[--n];
    const Def& d = kDefs[t];
    uint32_t span = d.hi - d.lo + 1;
    uint32_t target = d.lo + lcg(x) % span;
    if (d.hi > 2) target += target * (level > 40 ? 40 : level) / 40;  // up to 2x at level 40
    Quest& q = board.q[i];
    q.type = t;
    q.target = (uint16_t)target;
    q.base = metric(s, t);
    q.done = false;
  }
  board.active = true;
}

}  // namespace quests
