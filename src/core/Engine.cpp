#include "Engine.h"
#include <ArduinoJson.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <math.h>
#include "../hal/Gps.h"
#include "../hal/Storage.h"
#include "Achievements.h"
#include "config.h"

Engine engine;

namespace {
const char* kStatePath = "/scout/state.json";
const char* kSaltPath = "/scout/salt.bin";

uint32_t batchWifiNew = 0, batchBleNew = 0;

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
}  // namespace

// ---------------------------------------------------------------------------
bool Engine::begin() {
  loadSalt();
  loadState();
  size_t w = wifi_.load(), s = ssids_.load(), b = ble_.load(), c = cells_.load();
  log_i("engine: loaded %u wifi, %u ssid, %u ble, %u cells", w, s, b, c);
  // Counters are derived from the stores so the SD log is the source of truth.
  if (w) stats_.wifiUnique = w;
  if (s) stats_.ssidUnique = s;
  if (b) stats_.bleUnique = b;
  if (c) stats_.geoCells = c;
  stats_.sessions++;
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
  if (s.radio == Radio::WiFi) {
    if (s.rssi > stats_.bestRssi) { stats_.bestRssi = s.rssi; dirty_ = true; }

    if (s.channel && s.channel < 200 && !stats_.channels[s.channel]) {
      stats_.channels.set(s.channel);
      addXp(XP_NEW_CHANNEL);
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
    char extra[96];
    uint32_t now = gps::unixTime();
    if (gps::hasFix())
      snprintf(extra, sizeof(extra), "%016llx,%u,%s,%d,%lu,%.5f,%.5f", (unsigned long long)ssidId,
               s.channel, authName(s.auth), s.rssi, (unsigned long)now, gps::lat(), gps::lon());
    else
      snprintf(extra, sizeof(extra), "%016llx,%u,%s,%d,%lu,,", (unsigned long long)ssidId, s.channel,
               authName(s.auth), s.rssi, (unsigned long)now);

    if (wifi_.add(bssidId, extra)) {
      stats_.wifiUnique++;
      stats_.sessWifiNew++;
      batchWifiNew++;
      if (s.channel > 14) stats_.wifi5g++;
      switch (s.auth) {
        case AuthCat::Open: stats_.wifiOpen++; break;
        case AuthCat::WPA3: stats_.wifiWpa3++; break;
        case AuthCat::Enterprise: stats_.wifiEnterprise++; break;
        default: break;
      }
      addXp(s.auth == AuthCat::Enterprise ? XP_NEW_ENTERPRISE : XP_NEW_NETWORK);
    }
  } else if (s.radio == Radio::BLE) {
    stats_.bleSightings++;
    // Phones rotate private addresses every few minutes; only stable addresses
    // count as "unique devices", otherwise the counter is meaningless (and farmable).
    if (!s.stableAddr) return;
    uint64_t bleId = id(Radio::BLE, s.mac, 6);
    char extra[32];
    snprintf(extra, sizeof(extra), "%d,%lu", s.rssi, (unsigned long)gps::unixTime());
    if (ble_.add(bleId, extra)) {
      stats_.bleUnique++;
      stats_.sessBleNew++;
      batchBleNew++;
      addXp(XP_NEW_BLE);
    }
  }
  dirty_ = true;
}

void Engine::endScan(Radio radio, uint32_t seenThisScan) {
  stats_.scans++;
  if (radio == Radio::WiFi) {
    stats_.lastScanSeen = seenThisScan;
    if (batchWifiNew) {
      char t[40];
      snprintf(t, sizeof(t), batchWifiNew == 1 ? "New network!" : "%lu new networks!",
               (unsigned long)batchWifiNew);
      push(EventType::NewWifi, batchWifiNew, t);
    }
    batchWifiNew = 0;
  } else if (radio == Radio::BLE) {
    if (batchBleNew) {
      char t[40];
      snprintf(t, sizeof(t), "%lu new BLE device%s", (unsigned long)batchBleNew, batchBleNew == 1 ? "" : "s");
      push(EventType::NewBle, batchBleNew, t);
    }
    batchBleNew = 0;
  }
  wifi_.flush();
  ssids_.flush();
  ble_.flush();
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
    if (cells_.add(id(Radio::Thread /* unused tag */, key, 8) ^ 0x43454C4C00000000ULL, "")) {
      stats_.geoCells++;
      if (stats_.geoCells > 1) {
        addXp(XP_NEW_CELL);
        push(EventType::NewCell, stats_.geoCells, "New area explored!");
      }
      cells_.flush();
      checkAchievements();
      dirty_ = true;
    }
  }
}

// ---------------------------------------------------------------------------
void Engine::addXp(uint32_t xp) {
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

uint16_t Engine::level() const { return progression::levelForXp(stats_.xp); }

void Engine::checkAchievements() {
  for (size_t i = 0; i < ACHIEVEMENT_COUNT; i++) {
    if (stats_.achieved[i]) continue;
    if (ACHIEVEMENTS[i].check(stats_)) {
      stats_.achieved.set(i);
      addXp(XP_ACHIEVEMENT);
      push(EventType::Achievement, i, ACHIEVEMENTS[i].name);
    }
  }
}

void Engine::push(EventType t, uint32_t v, const char* text) {
  size_t next = (qHead_ + 1) % kQueue;
  if (next == qTail_) qTail_ = (qTail_ + 1) % kQueue;  // drop oldest
  queue_[qHead_].type = t;
  queue_[qHead_].value = v;
  strlcpy(queue_[qHead_].text, text, sizeof(queue_[qHead_].text));
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
  if (dirty_ && millis() - lastSaveMs_ > STATE_SAVE_INTERVAL_MS) saveNow();
}

void Engine::saveNow() {
  lastSaveMs_ = millis();
  if (!storage::ok()) return;
  JsonDocument doc;
  doc["v"] = 1;
  doc["fw"] = NG_FW_VERSION;
  doc["xp"] = stats_.xp;
  doc["scans"] = stats_.scans;
  doc["sessions"] = stats_.sessions;
  doc["wifi5g"] = stats_.wifi5g;
  doc["wifiOpen"] = stats_.wifiOpen;
  doc["wifiWpa3"] = stats_.wifiWpa3;
  doc["wifiEnterprise"] = stats_.wifiEnterprise;
  doc["bestRssi"] = stats_.bestRssi;
  doc["bleSightings"] = stats_.bleSightings;
  doc["lastDay"] = stats_.lastDay;
  doc["streak"] = stats_.streak;
  doc["bestStreak"] = stats_.bestStreak;

  JsonArray ch = doc["channels"].to<JsonArray>();
  for (size_t i = 0; i < stats_.channels.size(); i++)
    if (stats_.channels[i]) ch.add(i);

  JsonArray ach = doc["achievements"].to<JsonArray>();
  for (size_t i = 0; i < ACHIEVEMENT_COUNT; i++)
    if (stats_.achieved[i]) ach.add(ACHIEVEMENTS[i].id);

  JsonObject set = doc["settings"].to<JsonObject>();
  set["brightness"] = settings_.brightness;
  set["ble"] = settings_.bleScan;
  set["gps"] = settings_.gps;
  set["sound"] = settings_.sound;
  set["invert"] = settings_.invert;
  set["sprites"] = settings_.spritePack;

  String out;
  serializeJsonPretty(doc, out);
  if (storage::writeTextAtomic(kStatePath, out)) dirty_ = false;
}

void Engine::loadState() {
  String text = storage::readText(kStatePath);
  if (text.isEmpty()) return;
  JsonDocument doc;
  if (deserializeJson(doc, text)) {
    log_w("engine: state.json unreadable, starting fresh");
    return;
  }
  stats_.xp = doc["xp"] | 0;
  stats_.scans = doc["scans"] | 0;
  stats_.sessions = doc["sessions"] | 0;
  stats_.wifi5g = doc["wifi5g"] | 0;
  stats_.wifiOpen = doc["wifiOpen"] | 0;
  stats_.wifiWpa3 = doc["wifiWpa3"] | 0;
  stats_.wifiEnterprise = doc["wifiEnterprise"] | 0;
  stats_.bestRssi = doc["bestRssi"] | -127;
  stats_.bleSightings = doc["bleSightings"] | 0;
  stats_.lastDay = doc["lastDay"] | 0;
  stats_.streak = doc["streak"] | 0;
  stats_.bestStreak = doc["bestStreak"] | 0;

  for (JsonVariant v : doc["channels"].as<JsonArray>()) {
    int c = v.as<int>();
    if (c > 0 && c < 200) stats_.channels.set(c);
  }
  for (JsonVariant v : doc["achievements"].as<JsonArray>()) {
    const char* key = v.as<const char*>();
    if (!key) continue;
    for (size_t i = 0; i < ACHIEVEMENT_COUNT; i++)
      if (strcmp(key, ACHIEVEMENTS[i].id) == 0) stats_.achieved.set(i);
  }

  JsonObject set = doc["settings"];
  if (!set.isNull()) {
    settings_.brightness = set["brightness"] | 80;
    settings_.bleScan = set["ble"] | true;
    settings_.gps = set["gps"] | true;
    settings_.sound = set["sound"] | true;
    settings_.invert = set["invert"] | false;
    settings_.spritePack = set["sprites"] | "goblin";
  }
}
