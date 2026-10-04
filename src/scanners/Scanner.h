#pragma once
#include "../core/Types.h"

// Every radio is a Scanner. The ScanManager runs them one at a time (they share
// the single 2.4 GHz radio on the C5) and hands every Sighting to the Engine.
// Adding Zigbee/Thread later = one new class implementing this interface.
class Scanner {
 public:
  virtual ~Scanner() = default;
  virtual const char* name() const = 0;
  virtual Radio radio() const = 0;
  virtual bool begin() = 0;
  virtual bool enabled() const = 0;
  virtual void start() = 0;
  // Drain results. Returns true once the scan cycle is finished.
  // `seen` is the number of sightings delivered in this cycle.
  virtual bool poll(void (*sink)(const Sighting&), uint32_t& seen) = 0;
};
