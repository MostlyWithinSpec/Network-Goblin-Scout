#pragma once
#include <Arduino.h>
#include <esp_heap_caps.h>

// Open-addressing set of 64-bit ids, stored in PSRAM. Handles 100k+ entries cheaply.
// 0 is reserved as the empty marker (callers never produce id 0).
class HashSet64 {
 public:
  ~HashSet64() { if (table_) heap_caps_free(table_); }

  bool insert(uint64_t k) {  // true if newly added
    if (!table_ && !alloc(4096)) return false;
    if ((count_ + 1) * 10 > cap_ * 7) grow();
    size_t i = slot(k);
    while (table_[i]) {
      if (table_[i] == k) return false;
      i = (i + 1) & (cap_ - 1);
    }
    table_[i] = k;
    count_++;
    return true;
  }

  bool contains(uint64_t k) const {
    if (!table_) return false;
    size_t i = slot(k);
    while (table_[i]) {
      if (table_[i] == k) return true;
      i = (i + 1) & (cap_ - 1);
    }
    return false;
  }

  size_t size() const { return count_; }

 private:
  uint64_t* table_ = nullptr;
  size_t cap_ = 0, count_ = 0;

  size_t slot(uint64_t k) const {
    k ^= k >> 33; k *= 0xff51afd7ed558ccdULL; k ^= k >> 33;  // murmur finaliser
    return (size_t)k & (cap_ - 1);
  }

  bool alloc(size_t cap) {
    uint64_t* t = (uint64_t*)heap_caps_calloc(cap, sizeof(uint64_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!t) t = (uint64_t*)calloc(cap, sizeof(uint64_t));
    if (!t) return false;
    table_ = t; cap_ = cap; count_ = 0;
    return true;
  }

  void grow() {
    uint64_t* old = table_;
    size_t oldCap = cap_;
    if (!alloc(oldCap * 2)) { table_ = old; cap_ = oldCap; return; }
    for (size_t i = 0; i < oldCap; i++)
      if (old[i]) insert(old[i]);
    heap_caps_free(old);
  }
};
