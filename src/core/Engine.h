#pragma once
#include "SeenStore.h"
#include "Trackers.h"
#include "Types.h"

// The discovery -> XP -> achievement pipeline. Every scanner module feeds process();
// the engine doesn't care which radio a sighting came from.
class Engine {
 public:
  bool begin();
  void process(const Sighting& s);
  void endScan(Radio radio, uint32_t seenThisScan);  // flush + evaluate achievements
  void updateLocation();                             // call ~1 Hz; uses GPS if present
  void tick();                                       // periodic save, uptime
  void saveNow();
  void pet();                                        // the user tapped the goblin
  void setOnBattery(bool b) { onBattery_ = b; }      // counts battery minutes
  // Local wall-clock time if known (localUnix 0 = unknown): night minutes, seasonal hats.
  void setLocalTime(uint32_t localUnix) { localTime_ = localUnix; }
  void trackerIsMine();                              // last tracker alert was the owner's own
  void synced(uint32_t rank, uint32_t of);           // a leaderboard sync went through
  size_t trackersNearby() const { return watch_.nearby(millis()); }
  uint8_t hoardTier() const;                         // for the beacon (sniff-offs)
  // Beacon ids of goblins met (most recent last), for the leaderboard's encounter cross-check.
  // These are the goblins' own random public ids, not addresses.
  const uint32_t* metIds(size_t& count) const { count = metN_; return met_; }

  Stats& stats() { return stats_; }
  Settings& settings() { return settings_; }
  void settingsChanged() { dirty_ = true; saveNow(); }
  uint16_t level() const;

  const QuestBoard& quests() const { return board_; }
  Mood mood() const;
  uint32_t hatMask() const { return hatMask_; }
  // Recent sightings for the radar (ring buffer, newest anywhere).
  const Blip* blips(size_t& count) const { count = kBlips; return blips_; }

  // Goblins seen within the last `withinMs`.
  size_t nearbyPeers(uint32_t withinMs, const NearbyPeer** out, size_t max) const;

  bool popEvent(UiEvent& e);

 private:
  Stats stats_;
  Settings settings_;
  uint8_t salt_[16] = {0};
  bool dirty_ = false;
  bool dayChecked_ = false;
  bool saveSoon_ = false;
  bool onBattery_ = false;
  uint32_t localTime_ = 0;
  uint32_t lastSaveMs_ = 0;
  uint32_t lastMinuteMs_ = 0;
  uint32_t bonusAcc_ = 0;      // happy-goblin XP bonus, in quarter points
  uint32_t hatMask_ = 0;
  QuestBoard board_;

  static const size_t kBlips = 48;
  Blip blips_[kBlips];

  SeenStore wifi_{"/scout/wifi.csv"};
  SeenStore ssids_{"/scout/ssids.txt"};
  SeenStore ble_{"/scout/ble.csv"};
  SeenStore cells_{"/scout/cells.txt"};
  SeenStore t154_{"/scout/154.csv"};
  SeenStore pans_{"/scout/pans.txt"};
  SeenStore peers_{"/scout/peers.txt"};
  static const size_t kMet = 200;
  uint32_t met_[kMet] = {};
  size_t metN_ = 0;
  void rememberMet(uint32_t gid, bool save);
  SeenStore trackers_{"/scout/trackers.txt"};  // unique tracker addresses (salted), for the counter
  SeenStore trackersOk_{"/scout/trackers_ok.txt"};  // "it's mine": never alert for these

  trackers::PlaceTracker places_;
  trackers::Watch watch_;
  uint64_t lastAlertId_ = 0;
  struct SniffMemo { uint32_t id = 0, atMs = 0; };
  SniffMemo sniffMemo_[8];
  uint64_t scanTop_[trackers::PlaceTracker::kFp] = {};  // strongest networks in this Wi-Fi scan
  int8_t scanTopRssi_[trackers::PlaceTracker::kFp] = {};
  size_t scanTopN_ = 0;
  loot::Find best_{};         // rarest find of this scan (for the banner / rare-find overlay)
  uint32_t bestXp_ = 0;
  bool haveBest_ = false;
  bool bestBle_ = false;
  uint32_t blePopups_ = 0;    // "connect me" popup adverts from rotating addresses in this BLE scan
  uint32_t hackerAtMs_[loot::H_COUNT] = {};
  void addLoot(const loot::Find& f);
  uint32_t lootFind(const loot::Find& f, bool ble);  // count it, remember the best of the scan; returns XP
  int flushBest();                           // best find's rarity (-1 none); rare+ -> overlay event
  void spotHacker(uint8_t h);

  static const size_t kNearby = 6;
  NearbyPeer nearby_[kNearby];

  static const size_t kQueue = 16;
  UiEvent queue_[kQueue];
  size_t qHead_ = 0, qTail_ = 0;

  void loadSalt();
  void loadState();
  uint64_t id(Radio r, const uint8_t* data, size_t len) const;
  void addXp(uint32_t xp, bool discovery = false);
  void feed(int32_t hunger, int32_t boredom);  // negative = good
  void blip(uint64_t id, int8_t rssi, Radio r);
  void checkQuests();
  void checkAchievements();
  void push(EventType t, uint32_t v, const char* text, const NearbyPeer* p = nullptr);
  void processWifi(const Sighting& s);
  void processBle(const Sighting& s);
  void process154(const Sighting& s);
  void processPeer(const Sighting& s);
  void watchTracker(uint64_t id, const Sighting& s);
  void sniffOff(const NearbyPeer& p);
};

extern Engine engine;
