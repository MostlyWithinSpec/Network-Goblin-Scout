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

  // Named networks heard in the last couple of minutes, strongest first, for the Sync Wi-Fi
  // picker. RAM only. Returns how many were written to `out`.
  size_t recent(WifiChoice* out, size_t max) const;

 private:
  bool running_ = false;
  static const size_t kRecent = 12;
  WifiChoice recent_[kRecent] = {};
  void remember(const char* ssid, int8_t rssi, bool open);
};
