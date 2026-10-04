#pragma once
#include "Scanner.h"

// Placeholder for v1.1: passive 802.15.4 (Zigbee/Thread) discovery.
// Plan: use the C5's 802.15.4 radio in promiscuous mode, hop channels 11-26,
// count unique PAN IDs / extended addresses from beacons. Kept disabled until
// Wi-Fi + BLE are solid, since all three share the 2.4 GHz front end.
class ThreadScanner : public Scanner {
 public:
  const char* name() const override { return "802.15.4"; }
  Radio radio() const override { return Radio::Thread; }
  bool begin() override { return true; }
  bool enabled() const override { return false; }
  void start() override {}
  bool poll(void (*)(const Sighting&), uint32_t& seen) override { seen = 0; return true; }
};
