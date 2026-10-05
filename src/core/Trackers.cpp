#include "Trackers.h"

namespace trackers {

const char* kindName(uint8_t kind) {
  switch (kind) {
    case kFindMy: return "AirTag / Find My";
    case kSmartTag: return "Samsung SmartTag";
    case kTile: return "Tile";
    case kGoogle: return "Google Find My tag";
    case kChipolo: return "Chipolo";
    default: return "tracker";
  }
}

void PlaceTracker::wifiScan(const uint64_t* ids, size_t n) {
  if (n > kFp) n = kFp;
  if (n < 2) return;  // too little to judge (e.g. a quiet scan): keep the current place
  bool same = false;
  for (size_t i = 0; i < n && !same; i++)
    for (size_t j = 0; j < fpN_; j++)
      if (ids[i] == fp_[j]) { same = true; break; }
  // Still hearing one of this place's strongest networks: we haven't moved (much).
  if (same) return;
  if (fpN_) epoch_++;
  for (size_t i = 0; i < n; i++) fp_[i] = ids[i];
  fpN_ = n;
}

void PlaceTracker::gpsCell(uint64_t cell) {
  if (cell == cell_) return;
  if (cell_) epoch_++;
  cell_ = cell;
}

const Follower* Watch::see(uint64_t id, uint8_t kind, int8_t rssi, uint32_t place, uint32_t now) {
  Follower* slot = nullptr;
  Follower* victim = &f_[0];  // free slot, else the one seen in fewest places, then the stalest
  for (auto& f : f_) {
    if (f.id && now - f.lastMs > kForgetMs) f = Follower();  // it stayed behind
    if (f.id == id && id) slot = &f;
    if (!victim->id) continue;
    if (!f.id || f.places < victim->places || (f.places == victim->places && f.lastMs < victim->lastMs)) victim = &f;
  }
  if (!slot) {
    slot = victim;
    *slot = Follower();
    slot->id = id;
    slot->kind = kind;
    slot->firstMs = now;
  }
  slot->rssi = rssi;
  slot->lastMs = now;
  if (place != slot->lastPlace) {
    slot->lastPlace = place;
    if (slot->places < 255) slot->places++;
  }
  if (!slot->alerted && slot->places >= kMinPlaces && now - slot->firstMs >= kMinMs) {
    slot->alerted = true;
    return slot;
  }
  return nullptr;
}

void Watch::forget(uint64_t id) {
  for (auto& f : f_)
    if (f.id == id) f = Follower();
}

size_t Watch::nearby(uint32_t now) const {
  size_t n = 0;
  for (const auto& f : f_)
    if (f.id && now - f.lastMs < 120000) n++;
  return n;
}

}  // namespace trackers
