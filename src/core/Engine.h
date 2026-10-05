#pragma once
#include "SeenStore.h"
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
};

extern Engine engine;
