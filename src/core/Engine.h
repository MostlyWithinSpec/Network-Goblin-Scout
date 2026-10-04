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
  void tick();                                       // periodic save
  void saveNow();

  Stats& stats() { return stats_; }
  Settings& settings() { return settings_; }
  void settingsChanged() { dirty_ = true; saveNow(); }
  uint16_t level() const;

  bool popEvent(UiEvent& e);

 private:
  Stats stats_;
  Settings settings_;
  uint8_t salt_[16] = {0};
  bool dirty_ = false;
  bool dayChecked_ = false;
  uint32_t lastSaveMs_ = 0;

  SeenStore wifi_{"/scout/wifi.csv"};
  SeenStore ssids_{"/scout/ssids.txt"};
  SeenStore ble_{"/scout/ble.csv"};
  SeenStore cells_{"/scout/cells.txt"};

  static const size_t kQueue = 16;
  UiEvent queue_[kQueue];
  size_t qHead_ = 0, qTail_ = 0;

  void loadSalt();
  void loadState();
  uint64_t id(Radio r, const uint8_t* data, size_t len) const;
  void addXp(uint32_t xp);
  void checkAchievements();
  void push(EventType t, uint32_t v, const char* text);
};

extern Engine engine;
