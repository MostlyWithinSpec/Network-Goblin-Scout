#pragma once
#include "Scanner.h"

// Passive BLE advertisement scan (never sends scan requests or connects).
class BleScanner : public Scanner {
 public:
  explicit BleScanner(const bool* enabledFlag) : enabledFlag_(enabledFlag) {}
  const char* name() const override { return "BLE"; }
  Radio radio() const override { return Radio::BLE; }
  bool begin() override;
  bool enabled() const override { return enabledFlag_ && *enabledFlag_; }
  void start() override;
  bool poll(void (*sink)(const Sighting&), uint32_t& seen) override;

 private:
  const bool* enabledFlag_;
  bool ready_ = false;
  uint32_t cycleSeen_ = 0;
};
