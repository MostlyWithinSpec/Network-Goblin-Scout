#pragma once
#include <Arduino.h>
#include "HashSet64.h"

// A persistent "have we seen this id before?" set.
// Backed by an append-only text file on SD whose lines start with a hex id:
//   <16 hex id>,<anything else...>\n
// Only the id is loaded back into RAM; the rest of the line is a local log for the user.
class SeenStore {
 public:
  explicit SeenStore(const char* path) : path_(path) {}
  size_t load();                               // returns entries loaded
  bool add(uint64_t id, const String& extra);  // true if newly seen
  bool has(uint64_t id) const { return set_.contains(id); }
  void flush();                                // write pending lines to SD
  size_t size() const { return set_.size(); }

 private:
  const char* path_;
  HashSet64 set_;
  String pending_;
};
