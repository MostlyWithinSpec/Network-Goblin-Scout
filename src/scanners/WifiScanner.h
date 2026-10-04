#pragma once
#include "Scanner.h"

// Passive dual-band Wi-Fi scan: listens for beacons, never transmits probes.
class WifiScanner : public Scanner {
 public:
  const char* name() const override { return "Wi-Fi"; }
  Radio radio() const override { return Radio::WiFi; }
  bool begin() override;
  bool enabled() const override { return true; }
  void start() override;
  bool poll(void (*sink)(const Sighting&), uint32_t& seen) override;

 private:
  bool running_ = false;
};
