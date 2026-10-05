#pragma once
#include <stddef.h>
#include <stdint.h>

// "Is something following me?" Pure logic (no Arduino), unit-tested in test/test_trackers.cpp.
//
// BleScanner tags adverts that look like item trackers (kind below). Each one is followed by
// its salted id, in RAM only. It raises an alert once when the same tracker has been with us
// in at least kMinPlaces different places over at least kMinMs. A "place" changes when the
// GPS cell changes or, without GPS, when none of the strongest Wi-Fi networks from the last
// place are heard any more (PlaceTracker).
//
// Works for trackers that keep their address while away from their owner: AirTags and other
// Find My accessories (same address for up to a day), Tile, Google Find My Device tags in
// "separated" mode. Trackers that change address more often can slip through.
namespace trackers {

enum Kind : uint8_t { kNone = 0, kFindMy, kSmartTag, kTile, kGoogle, kChipolo, kKindCount };
const char* kindName(uint8_t kind);

const uint32_t kMinMs = 15UL * 60 * 1000;   // with us for at least this long...
const uint8_t kMinPlaces = 3;               // ...in at least this many places
const uint32_t kForgetMs = 20UL * 60 * 1000; // not heard this long: it isn't following us

class PlaceTracker {
 public:
  // After each Wi-Fi scan: the salted ids of up to kFp strongest networks heard.
  void wifiScan(const uint64_t* ids, size_t n);
  // Every GPS update with a fix: the exploration cell id.
  void gpsCell(uint64_t cell);
  uint32_t place() const { return epoch_; }

  static const size_t kFp = 6;

 private:
  uint64_t fp_[kFp] = {};
  size_t fpN_ = 0;
  uint64_t cell_ = 0;
  uint32_t epoch_ = 1;
};

struct Follower {
  uint64_t id = 0;
  uint8_t kind = kNone;
  int8_t rssi = -127;
  uint8_t places = 0;
  bool alerted = false;
  uint32_t lastPlace = 0;
  uint32_t firstMs = 0, lastMs = 0;
};

class Watch {
 public:
  // Feed every tracker sighting. Returns the follower when it has just crossed the alert
  // threshold (once per follower), otherwise nullptr.
  const Follower* see(uint64_t id, uint8_t kind, int8_t rssi, uint32_t place, uint32_t now);
  void forget(uint64_t id);
  // Trackers heard in the last 2 minutes.
  size_t nearby(uint32_t now) const;

  static const size_t kSlots = 32;

 private:
  Follower f_[kSlots];
};

}  // namespace trackers
