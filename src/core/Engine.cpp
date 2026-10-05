#include "Engine.h"
#include <ArduinoJson.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <math.h>
#include <initializer_list>
#include "../hal/Gps.h"
#include "../hal/Storage.h"
#include "Achievements.h"
#include "Hats.h"
#include "Quests.h"
#include "../social/Sniff.h"
#include "config.h"

Engine engine;

namespace {
const char* kStatePath = "/scout/state.json";
const char* kSaltPath = "/scout/salt.bin";

uint32_t batchWifiNew = 0, batchBleNew = 0, batch154New = 0;

const char* authName(AuthCat a) {
  switch (a) {
    case AuthCat::Open: return "open";
    case AuthCat::WEP: return "wep";
    case AuthCat::WPA: return "wpa";
    case AuthCat::WPA3: return "wpa3";
    case AuthCat::Enterprise: return "ent";
    default: return "other";
  }
}

bool isDfs(uint8_t ch) { return ch >= 52 && ch <= 144; }
}  // namespace

// ---------------------------------------------------------------------------
bool Engine::begin() {
  loadSalt();
  loadState();
  size_t w = wifi_.load(), s = ssids_.load(), b = ble_.load(), c = cells_.load();
  size_t t = t154_.load(), p = pans_.load(), g = peers_.load();
  trackers_.load();
  trackersOk_.load();
  log_i("engine: loaded %u wifi, %u ssid, %u ble, %u cells, %u 802.15.4, %u pans, %u goblins", w, s, b, c, t,
        p, g);
  // Counters are derived from the stores so the SD log is the source of truth.
  stats_.wifiUnique = w;
  stats_.ssidUnique = s;
  stats_.bleUnique = b;
  stats_.geoCells = c;
  stats_.t154Unique = t;
  stats_.t154Pans = p;
  stats_.peersMet = g;
  stats_.sessions++;
  lastMinuteMs_ = millis();
  hatMask_ = hats::unlockedMask(stats_, level());
  if (!board_.active && stats_.uptimeMin >= board_.nextAtMin) {
    quests::deal(stats_, level(), esp_random(), board_);
    push(EventType::NewQuests, 0, "New quests on the board!");
  }
  checkAchievements();
  dirty_ = true;
  return true;
}

// Per-device random salt: ids on SD are salted hashes, never raw MACs.
void Engine::loadSalt() {
  if (storage::readBytes(kSaltPath, salt_, sizeof(salt_))) return;
  esp_fill_random(salt_, sizeof(salt_));
  storage::writeBytes(kSaltPath, salt_, sizeof(salt_));
}

uint64_t Engine::id(Radio r, const uint8_t* data, size_t len) const {
  uint8_t in[16 + 1 + 64];
  if (len > 64) len = 64;
  memcpy(in, salt_, 16);
  in[16] = (uint8_t)r;
  memcpy(in + 17, data, len);
  uint8_t out[32];
  mbedtls_sha256(in, 17 + len, out, 0);
  uint64_t v;
  memcpy(&v, out, 8);
  return v ? v : 1;
}

// ---------------------------------------------------------------------------
void Engine::process(const Sighting& s) {
  switch (s.radio) {
    case Radio::WiFi: processWifi(s); break;
    case Radio::BLE: processBle(s); break;
    case Radio::Thread: process154(s); break;
    case Radio::Peer: processPeer(s); break;
  }
  dirty_ = true;
}

void Engine::processWifi(const Sighting& s) {
  if (s.rssi > stats_.bestRssi) stats_.bestRssi = s.rssi;
  if (s.rssi < 0 && (stats_.worstRssi == 0 || s.rssi < stats_.worstRssi)) stats_.worstRssi = s.rssi;

  if (s.channel && s.channel < 200 && !stats_.channels[s.channel]) {
    stats_.channels.set(s.channel);
    addXp(XP_NEW_CHANNEL, true);
    feed(0, -150);
    char t[40];
    snprintf(t, sizeof(t), "New channel %u!", s.channel);
    push(EventType::NewChannel, s.channel, t);
  }

  uint64_t ssidId = 0;
  size_t nameLen = strnlen(s.name, 32);
  if (nameLen) {
    ssidId = id(Radio::WiFi, (const uint8_t*)s.name, nameLen) ^ 0x5353494400000000ULL;
    if (ssids_.add(ssidId, "")) stats_.ssidUnique++;
  }

  uint64_t bssidId = id(Radio::WiFi, s.mac, 6);
  blip(bssidId, s.rssi, Radio::WiFi);
  // keep the strongest few of this scan: they fingerprint "where we are" for the tracker alert
  {
    size_t i = scanTopN_ < trackers::PlaceTracker::kFp ? scanTopN_++ : trackers::PlaceTracker::kFp;
    if (i == trackers::PlaceTracker::kFp) {  // full: replace the weakest if this one is stronger
      size_t w = 0;
      for (size_t k = 1; k < trackers::PlaceTracker::kFp; k++)
        if (scanTopRssi_[k] < scanTopRssi_[w]) w = k;
      if (s.rssi > scanTopRssi_[w]) i = w;
    }
    if (i < trackers::PlaceTracker::kFp) {
      scanTop_[i] = bssidId;
      scanTopRssi_[i] = s.rssi;
    }
  }
  char extra[96];
  uint32_t now = gps::unixTime();
  if (gps::hasFix())
    snprintf(extra, sizeof(extra), "%016llx,%u,%s,%d,%lu,%.5f,%.5f", (unsigned long long)ssidId, s.channel,
             authName(s.auth), s.rssi, (unsigned long)now, gps::lat(), gps::lon());
  else
    snprintf(extra, sizeof(extra), "%016llx,%u,%s,%d,%lu,,", (unsigned long long)ssidId, s.channel,
             authName(s.auth), s.rssi, (unsigned long)now);

  if (!wifi_.add(bssidId, extra)) return;
  stats_.wifiUnique++;
  stats_.sessWifiNew++;
  batchWifiNew++;
  if (s.channel > 14) stats_.wifi5g++;
  if (isDfs(s.channel)) stats_.wifiDfs++;
  if (s.flags & sflag::kWifi6) stats_.wifi6++;
  if (s.flags & sflag::kWps) stats_.wifiWps++;
  if (s.flags & sflag::kHidden) stats_.wifiHidden++;
  switch (s.auth) {
    case AuthCat::Open: stats_.wifiOpen++; break;
    case AuthCat::WEP: stats_.wifiWep++; break;
    case AuthCat::WPA3: stats_.wifiWpa3++; break;
    case AuthCat::Enterprise: stats_.wifiEnterprise++; break;
    default: break;
  }
  addXp(s.auth == AuthCat::Enterprise ? XP_NEW_ENTERPRISE : XP_NEW_NETWORK, true);
  feed(-15, -4);
}

void Engine::processBle(const Sighting& s) {
  stats_.bleSightings++;
  uint64_t anyId = id(Radio::BLE, s.mac, 6);
  blip(anyId, s.rssi, Radio::BLE);
  if (s.tracker) watchTracker(anyId, s);
  // Phones rotate private addresses every few minutes; only stable addresses
  // count as "unique devices", otherwise the counter is meaningless (and farmable).
  if (!s.stableAddr) return;
  uint64_t bleId = anyId;
  char extra[32];
  snprintf(extra, sizeof(extra), "%d,%lu", s.rssi, (unsigned long)gps::unixTime());
  if (!ble_.add(bleId, extra)) return;
  stats_.bleUnique++;
  stats_.sessBleNew++;
  batchBleNew++;
  if (s.flags & sflag::kNamed) stats_.bleNamed++;
  if (s.flags & sflag::kIBeacon) stats_.bleIBeacon++;
  if (s.flags & sflag::kEddystone) stats_.bleEddystone++;
  addXp(XP_NEW_BLE, true);
  feed(-8, -2);
}

void Engine::process154(const Sighting& s) {
  stats_.t154Frames++;
  if (s.channel >= 11 && s.channel <= 26) stats_.channels154.set(s.channel);
  const char* kind = (s.flags & sflag::kZigbee) ? "zigbee" : (s.flags & sflag::kThread) ? "thread" : "?";

  uint64_t panId = 0;
  if (s.panId != 0xFFFF) {
    uint8_t key[3] = {(uint8_t)s.panId, (uint8_t)(s.panId >> 8), s.channel};
    panId = id(Radio::Thread, key, 3) ^ 0x50414E0000000000ULL;
    char extra[24];
    snprintf(extra, sizeof(extra), "%u,%s", s.channel, kind);
    if (pans_.add(panId, extra)) {
      stats_.t154Pans++;
      addXp(XP_NEW_PAN, true);
      feed(0, -200);
      const char* t = "New 802.15.4 network!";
      if (s.flags & sflag::kZigbee) { stats_.zigbeePans++; t = "New Zigbee network!"; }
      if (s.flags & sflag::kThread) { stats_.threadPans++; t = "New Thread network!"; }
      push(EventType::New154, stats_.t154Pans, t);
      pans_.flush();
    }
  }

  uint64_t devId = id(Radio::Thread, s.mac, s.macLen);
  blip(devId, s.rssi, Radio::Thread);
  char extra[64];
  snprintf(extra, sizeof(extra), "%016llx,%u,%s,%d,%lu", (unsigned long long)panId, s.channel, kind, s.rssi,
           (unsigned long)gps::unixTime());
  if (!t154_.add(devId, extra)) return;
  stats_.t154Unique++;
  stats_.sess154New++;
  batch154New++;
  addXp(XP_NEW_154, true);
  feed(-20, -5);
}

void Engine::processPeer(const Sighting& s) {
  uint32_t now = millis();
  uint32_t pid;
  memcpy(&pid, s.mac, 4);

  NearbyPeer* slot = nullptr;
  for (auto& n : nearby_)
    if (n.id == pid) slot = &n;
  bool encounter = !slot || now - slot->lastSeenMs > PEER_REVISIT_MS;
  if (!slot) {  // reuse the stalest slot
    slot = &nearby_[0];
    for (auto& n : nearby_)
      if (n.id == 0 || n.lastSeenMs < slot->lastSeenMs) slot = &n;
  }
  slot->id = pid;
  strlcpy(slot->name, s.name, sizeof(slot->name));
  slot->level = s.peerLevel;
  slot->hue = s.peerHue;
  slot->rssi = s.rssi;
  slot->hat = s.flags;
  slot->hoard = s.peerHoard;
  blip(id(Radio::Peer, s.mac, 4), s.rssi, Radio::Peer);
  slot->lastSeenMs = now;
  if (!encounter) return;

  stats_.peerEncounters++;
  char extra[24];
  snprintf(extra, sizeof(extra), "%u,%lu", s.peerLevel, (unsigned long)gps::unixTime());
  bool isNew = peers_.add(id(Radio::Peer, s.mac, 4), extra);
  peers_.flush();
  if (isNew) stats_.peersMet++;
  if (s.peerLevel > level()) stats_.metHigherLevel = 1;
  if (s.rssi >= -45) stats_.closeEncounter = 1;
  const NearbyPeer* tmp[kNearby];
  uint32_t together = nearbyPeers(PEER_TOGETHER_MS, tmp, kNearby);
  if (together > stats_.maxPeersAtOnce) stats_.maxPeersAtOnce = together;

  char t[40];
  snprintf(t, sizeof(t), isNew ? "Met %s!" : "%s is back!", slot->name);
  push(isNew ? EventType::PeerNew : EventType::PeerReunion, s.peerLevel, t, slot);
  addXp(isNew ? XP_NEW_PEER : XP_PEER_REUNION);
  feed(-100, -400);
  sniffOff(*slot);
  checkAchievements();
}

void Engine::sniffOff(const NearbyPeer& p) {
  // Once per goblin every few hours, so two Scouts can't farm it by stepping in and out of range.
  uint32_t now = millis();
  SniffMemo* memo = &sniffMemo_[0];
  for (auto& m : sniffMemo_) {
    if (m.id == p.id) { memo = &m; break; }
    if (m.atMs < memo->atMs) memo = &m;
  }
  if (memo->id == p.id && now - memo->atMs < SNIFF_COOLDOWN_MS) return;
  memo->id = p.id;
  memo->atMs = now ? now : 1;

  uint8_t mine = hoardTier();
  sniff::Result r = sniff::judge(mine, level(), p.hoard, p.level);
  stats_.sniffOffs++;
  if (r == sniff::kWin) stats_.sniffWins++;
  uint32_t xp = r == sniff::kWin ? XP_SNIFF_WIN : r == sniff::kDraw ? XP_SNIFF_DRAW : XP_SNIFF_LOSE;
  char t[40];
  snprintf(t, sizeof(t), r == sniff::kWin ? "Sniff-off won! +%lu XP" : r == sniff::kDraw ? "Sniff-off draw! +%lu XP"
                                                                                       : "Sniff-off lost. +%lu XP",
           (unsigned long)xp);
  // value: result | my tier << 8 | their tier << 16 | xp << 24 (the UI unpacks it)
  push(EventType::SniffOff, r | (uint32_t)mine << 8 | (uint32_t)p.hoard << 16 | (xp > 255 ? 255 : xp) << 24, t, &p);
  addXp(xp);
}

uint8_t Engine::hoardTier() const {
  return sniff::tier(stats_.wifiUnique + stats_.bleUnique + stats_.t154Unique);
}

void Engine::watchTracker(uint64_t tid, const Sighting& s) {
  char extra[24];
  snprintf(extra, sizeof(extra), "%s,%lu", trackers::kindName(s.tracker), (unsigned long)gps::unixTime());
  if (trackers_.add(tid, extra)) {
    stats_.trackersSeen++;
    trackers_.flush();
  }
  if (trackersOk_.has(tid)) return;
  uint32_t now = millis();
  const trackers::Follower* f = watch_.see(tid, s.tracker, s.rssi, places_.place(), now ? now : 1);
  if (!f) return;
  stats_.trackerAlerts++;
  lastAlertId_ = f->id;
  uint32_t mins = (f->lastMs - f->firstMs) / 60000;
  // value: kind | places << 8 | minutes << 16 (the UI unpacks it)
  push(EventType::TrackerAlert, f->kind | (uint32_t)f->places << 8 | (mins > 0xFFFF ? 0xFFFF : mins) << 16,
       "Tracker following you!");
  log_w("tracker: a %s has followed us through %u places for %lu min", trackers::kindName(f->kind), f->places,
        (unsigned long)mins);
  checkAchievements();
}

void Engine::trackerIsMine() {
  if (!lastAlertId_) return;
  trackersOk_.add(lastAlertId_, "mine");
  trackersOk_.flush();
  watch_.forget(lastAlertId_);
  lastAlertId_ = 0;
}

size_t Engine::nearbyPeers(uint32_t withinMs, const NearbyPeer** out, size_t max) const {
  uint32_t now = millis();
  size_t n = 0;
  for (const auto& p : nearby_)
    if (p.id && now - p.lastSeenMs <= withinMs && n < max) out[n++] = &p;
  return n;
}

void Engine::endScan(Radio radio, uint32_t seenThisScan) {
  stats_.scans++;
  char t[40];
  if (radio == Radio::WiFi) {
    places_.wifiScan(scanTop_, scanTopN_);
    scanTopN_ = 0;
    stats_.lastScanSeen = seenThisScan;
    if (seenThisScan > stats_.maxApsInScan) stats_.maxApsInScan = seenThisScan;
    if (batchWifiNew) {
      snprintf(t, sizeof(t), batchWifiNew == 1 ? "New network!" : "%lu new networks!", (unsigned long)batchWifiNew);
      push(EventType::NewWifi, batchWifiNew, t);
    }
    batchWifiNew = 0;
  } else if (radio == Radio::BLE) {
    stats_.lastBleSeen = seenThisScan;
    if (seenThisScan > stats_.maxBleInScan) stats_.maxBleInScan = seenThisScan;
    if (batchBleNew) {
      snprintf(t, sizeof(t), "%lu new BLE device%s", (unsigned long)batchBleNew, batchBleNew == 1 ? "" : "s");
      push(EventType::NewBle, batchBleNew, t);
    }
    batchBleNew = 0;
  } else if (radio == Radio::Thread) {
    stats_.last154Seen = seenThisScan;
    if (batch154New) {
      snprintf(t, sizeof(t), "%lu new mesh device%s", (unsigned long)batch154New, batch154New == 1 ? "" : "s");
      push(EventType::New154, batch154New, t);
    }
    batch154New = 0;
  }
  wifi_.flush();
  ssids_.flush();
  ble_.flush();
  t154_.flush();
  checkAchievements();
  dirty_ = true;
}

void Engine::pet() {
  stats_.pets++;
  feed(0, -10);
  checkAchievements();
  dirty_ = true;
}

// ---------------------------------------------------------------------------
void Engine::updateLocation() {
  // Daily streak — needs real time, which on this board only comes from GPS.
  if (!dayChecked_ && gps::timeValid()) {
    dayChecked_ = true;
    uint32_t day = gps::unixTime() / 86400UL;
    if (day != stats_.lastDay) {
      stats_.streak = (stats_.lastDay && day == stats_.lastDay + 1) ? stats_.streak + 1 : 1;
      if (stats_.streak > stats_.bestStreak) stats_.bestStreak = stats_.streak;
      stats_.lastDay = day;
      addXp(XP_DAILY_BONUS);
      char t[40];
      snprintf(t, sizeof(t), "Daily bonus! Streak %lu", (unsigned long)stats_.streak);
      push(EventType::DailyBonus, stats_.streak, t);
      checkAchievements();
      dirty_ = true;
    }
  }

  // Exploration cells: coarse grid squares, kept on-device only.
  if (gps::hasFix()) {
    int32_t cy = (int32_t)floor(gps::lat() / GEO_CELL_DEG);
    int32_t cx = (int32_t)floor(gps::lon() / GEO_CELL_DEG);
    uint8_t key[8];
    memcpy(key, &cy, 4);
    memcpy(key + 4, &cx, 4);
    uint64_t cell = id(Radio::Thread /* historical tag, keep */, key, 8) ^ 0x43454C4C00000000ULL;
    places_.gpsCell(cell);
    if (cells_.add(cell, "")) {
      stats_.geoCells++;
      if (stats_.geoCells > 1) {
        addXp(XP_NEW_CELL, true);
        feed(0, -250);
        push(EventType::NewCell, stats_.geoCells, "New area explored!");
      }
      cells_.flush();
      checkAchievements();
      dirty_ = true;
    }
  }
}

// ---------------------------------------------------------------------------
void Engine::addXp(uint32_t xp, bool discovery) {
  // A happy goblin (fed and entertained) earns +25% on discoveries.
  if (discovery && mood() == Mood::Happy) {
    bonusAcc_ += xp;
    xp += bonusAcc_ / 4;
    bonusAcc_ %= 4;
  }
  uint16_t before = progression::levelForXp(stats_.xp);
  stats_.xp += xp;
  stats_.sessXp += xp;
  uint16_t after = progression::levelForXp(stats_.xp);
  if (after > before) {
    char t[40];
    snprintf(t, sizeof(t), "Level %u! %s", after, progression::title(after));
    push(EventType::LevelUp, after, t);
  }
}

void Engine::feed(int32_t hunger, int32_t boredom) {
  auto adj = [](uint32_t& v, int32_t d) {
    int32_t n = (int32_t)v + d;
    v = (uint32_t)(n < 0 ? 0 : (n > 1000 ? 1000 : n));
  };
  adj(stats_.hunger, hunger);
  adj(stats_.boredom, boredom);
}

Mood Engine::mood() const {
  if (stats_.hunger >= 850) return Mood::Starving;
  if (stats_.hunger >= 600) return Mood::Hungry;
  if (stats_.boredom >= 650) return Mood::Bored;
  if (stats_.hunger < 300 && stats_.boredom < 300) return Mood::Happy;
  return Mood::Content;
}

void Engine::blip(uint64_t idv, int8_t rssi, Radio r) {
  uint16_t angle = (uint16_t)((idv >> 20) & 4095);
  uint32_t now = millis();
  Blip* slot = &blips_[0];
  for (auto& b : blips_) {
    if (b.lastSeenMs && b.angle == angle && b.radio == r) { slot = &b; break; }
    if (b.lastSeenMs < slot->lastSeenMs) slot = &b;  // else reuse the oldest
  }
  slot->angle = angle;
  slot->rssi = rssi;
  slot->radio = r;
  slot->lastSeenMs = now ? now : 1;
}

void Engine::checkQuests() {
  if (!board_.active) return;
  bool all = true;
  for (auto& q : board_.q) {
    if (q.done) continue;
    if (quests::progress(stats_, q) >= q.target) {
      q.done = true;
      stats_.questsDone++;
      char t[40];
      snprintf(t, sizeof(t), "Quest done! +%lu XP", (unsigned long)quests::reward(q));
      push(EventType::QuestDone, quests::reward(q), t);
      addXp(quests::reward(q));
      feed(0, -200);
      dirty_ = true;
    } else {
      all = false;
    }
  }
  if (all) {
    stats_.boardsCleared++;
    board_.active = false;
    board_.nextAtMin = stats_.uptimeMin + QUEST_COOLDOWN_MIN;
    addXp(XP_BOARD_CLEARED);
    push(EventType::BoardCleared, XP_BOARD_CLEARED, "Quest board cleared!");
  }
}

uint16_t Engine::level() const { return progression::levelForXp(stats_.xp); }

void Engine::checkAchievements() {
  checkQuests();
  uint32_t mask = hats::unlockedMask(stats_, level());
  if (uint32_t fresh = mask & ~hatMask_) {
    for (uint8_t i = 0; i < hats::kCount; i++)
      if (fresh & (1u << i)) {
        char t[40];
        snprintf(t, sizeof(t), "New hat: %s!", hats::kHats[i].name);
        push(EventType::HatUnlocked, i + 1, t);
      }
    hatMask_ = mask;
  }
  for (size_t i = 0; i < ACHIEVEMENT_COUNT; i++) {
    if (stats_.achieved[i]) continue;
    if (ACHIEVEMENTS[i].check(stats_)) {
      stats_.achieved.set(i);
      addXp(XP_ACHIEVEMENT * (1 + ACHIEVEMENTS[i].tier));
      push(EventType::Achievement, i, ACHIEVEMENTS[i].name);
    }
  }
}

void Engine::push(EventType t, uint32_t v, const char* text, const NearbyPeer* p) {
  // Big moments are saved within a few seconds instead of waiting for the periodic save,
  // so unplugging (or re-flashing) right after one doesn't lose it.
  if (t == EventType::LevelUp || t == EventType::Achievement || t == EventType::QuestDone ||
      t == EventType::BoardCleared || t == EventType::HatUnlocked || t == EventType::PeerNew)
    saveSoon_ = true;
  size_t next = (qHead_ + 1) % kQueue;
  if (next == qTail_) qTail_ = (qTail_ + 1) % kQueue;  // drop oldest
  UiEvent& e = queue_[qHead_];
  e.type = t;
  e.value = v;
  strlcpy(e.text, text, sizeof(e.text));
  e.peerName[0] = 0;
  e.peerLevel = 0;
  e.peerHue = 0;
  e.peerHat = 0;
  e.peerHoard = 0;
  if (p) {
    strlcpy(e.peerName, p->name, sizeof(e.peerName));
    e.peerLevel = p->level;
    e.peerHue = p->hue;
    e.peerHat = p->hat;
    e.peerHoard = p->hoard;
  }
  qHead_ = next;
}

bool Engine::popEvent(UiEvent& e) {
  if (qTail_ == qHead_) return false;
  e = queue_[qTail_];
  qTail_ = (qTail_ + 1) % kQueue;
  return true;
}

// ---------------------------------------------------------------------------
void Engine::tick() {
  uint32_t now = millis();
  if (now - lastMinuteMs_ >= 60000) {
    lastMinuteMs_ += 60000;
    stats_.uptimeMin++;
    if (onBattery_) stats_.batteryMin++;
    feed(HUNGER_PER_MIN, BOREDOM_PER_MIN);  // the goblin gets hungry and bored over time
    if (!board_.active && stats_.uptimeMin >= board_.nextAtMin) {
      quests::deal(stats_, level(), esp_random(), board_);
      push(EventType::NewQuests, 0, "New quests on the board!");
    }
    checkAchievements();
    dirty_ = true;
  }
  if (saveSoon_ && now - lastSaveMs_ > 3000) saveNow();
  if (dirty_ && now - lastSaveMs_ > STATE_SAVE_INTERVAL_MS) saveNow();
}

void Engine::saveNow() {
  lastSaveMs_ = millis();
  saveSoon_ = false;
  if (!storage::ok()) return;
  JsonDocument doc;
  doc["v"] = 2;
  doc["fw"] = NG_FW_VERSION;
#define NG_SAVE_COUNTER(n) doc[#n] = stats_.n;
  NG_SAVED_COUNTERS(NG_SAVE_COUNTER)
#undef NG_SAVE_COUNTER
  doc["bestRssi"] = stats_.bestRssi;
  doc["worstRssi"] = stats_.worstRssi;

  JsonArray ch = doc["channels"].to<JsonArray>();
  for (size_t i = 0; i < stats_.channels.size(); i++)
    if (stats_.channels[i]) ch.add(i);
  JsonArray ch154 = doc["channels154"].to<JsonArray>();
  for (size_t i = 0; i < stats_.channels154.size(); i++)
    if (stats_.channels154[i]) ch154.add(i);

  JsonObject qb = doc["quests"].to<JsonObject>();
  qb["active"] = board_.active;
  qb["next"] = board_.nextAtMin;
  JsonArray qa = qb["q"].to<JsonArray>();
  for (const auto& q : board_.q) {
    JsonObject o = qa.add<JsonObject>();
    o["t"] = q.type;
    o["n"] = q.target;
    o["b"] = q.base;
    o["d"] = q.done;
  }

  JsonArray ach = doc["achievements"].to<JsonArray>();
  for (size_t i = 0; i < ACHIEVEMENT_COUNT; i++)
    if (stats_.achieved[i]) ach.add(ACHIEVEMENTS[i].id);

  JsonObject set = doc["settings"].to<JsonObject>();
  set["brightness"] = settings_.brightness;
  set["ble"] = settings_.bleScan;
  set["154"] = settings_.scan154;
  set["beacon"] = settings_.beacon;
  set["gps"] = settings_.gps;
  set["sound"] = settings_.sound;
  set["invert"] = settings_.invert;
  set["fastDisplay"] = settings_.fastDisplay;
  set["hat"] = settings_.hat;
  set["goblinId"] = settings_.goblinId;
  set["name"] = settings_.goblinName;
  set["agreed"] = settings_.agreed;
  set["sprites"] = settings_.spritePack;

  String out;
  serializeJsonPretty(doc, out);
  if (storage::writeTextAtomic(kStatePath, out)) dirty_ = false;
}

void Engine::loadState() {
  // state.json, else the backup from the last save, else a finished temp file.
  JsonDocument doc;
  bool ok = false;
  for (const char* suffix : {"", ".bak", ".tmp"}) {
    String path = String(kStatePath) + suffix;
    String text = storage::readText(path.c_str());
    if (text.isEmpty()) continue;
    if (deserializeJson(doc, text)) {
      log_w("engine: %s unreadable", path.c_str());
      continue;
    }
    if (suffix[0]) log_w("engine: recovered progress from %s", path.c_str());
    ok = true;
    break;
  }
  if (!ok) return;
#define NG_LOAD_COUNTER(n) stats_.n = doc[#n] | 0;
  NG_SAVED_COUNTERS(NG_LOAD_COUNTER)
#undef NG_LOAD_COUNTER
  stats_.bestRssi = doc["bestRssi"] | -127;
  stats_.worstRssi = doc["worstRssi"] | 0;

  for (JsonVariant v : doc["channels"].as<JsonArray>()) {
    int c = v.as<int>();
    if (c > 0 && c < 200) stats_.channels.set(c);
  }
  for (JsonVariant v : doc["channels154"].as<JsonArray>()) {
    int c = v.as<int>();
    if (c >= 11 && c <= 26) stats_.channels154.set(c);
  }
  for (JsonVariant v : doc["achievements"].as<JsonArray>()) {
    const char* key = v.as<const char*>();
    if (!key) continue;
    for (size_t i = 0; i < ACHIEVEMENT_COUNT; i++)
      if (strcmp(key, ACHIEVEMENTS[i].id) == 0) stats_.achieved.set(i);
  }

  JsonObject qb = doc["quests"];
  if (!qb.isNull()) {
    board_.active = qb["active"] | false;
    board_.nextAtMin = qb["next"] | 0;
    size_t i = 0;
    JsonArray qa = qb["q"];
    for (JsonObject o : qa) {
      if (i >= 3) break;
      Quest& q = board_.q[i++];
      q.type = o["t"] | 0;
      q.target = o["n"] | 1;
      q.base = o["b"] | 0;
      q.done = o["d"] | false;
      if (q.type >= quests::kTypeCount) board_.active = false;  // from a newer firmware: re-deal
    }
  }

  JsonObject set = doc["settings"];
  if (!set.isNull()) {
    settings_.brightness = set["brightness"] | 80;
    settings_.bleScan = set["ble"] | true;
    settings_.scan154 = set["154"] | true;
    settings_.beacon = set["beacon"] | true;
    settings_.gps = set["gps"] | true;
    settings_.sound = set["sound"] | true;
    settings_.invert = set["invert"] | false;
    settings_.fastDisplay = set["fastDisplay"] | true;
    settings_.hat = set["hat"] | 0;
    settings_.goblinId = set["goblinId"] | 0u;  // 0u: ids above 2^31 don't fit an int default
    settings_.goblinName = set["name"] | "";
    settings_.agreed = set["agreed"] | false;
    settings_.spritePack = set["sprites"] | "goblin";
  }
}
