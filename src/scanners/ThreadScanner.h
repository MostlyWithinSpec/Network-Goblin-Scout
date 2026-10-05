#pragma once
#include "Scanner.h"

// Passive 802.15.4 (Zigbee / Thread / Matter-over-Thread) listener.
// Puts the C5's 802.15.4 radio in promiscuous receive mode and hops channels 11-26,
// never transmitting. Each frame with a source address becomes a Sighting; the
// MAC header is parsed by Ieee802154Frame.h to find the PAN and guess Zigbee vs Thread.
// The radio is only enabled during this scanner's turn, since it shares the 2.4 GHz
// front end with Wi-Fi and BLE.
class ThreadScanner : public Scanner {
 public:
  explicit ThreadScanner(const bool* enabledFlag) : enabledFlag_(enabledFlag) {}
  const char* name() const override { return "802.15.4"; }
  Radio radio() const override { return Radio::Thread; }
  bool begin() override;
  bool enabled() const override { return ready_ && enabledFlag_ && *enabledFlag_; }
  void start() override;
  bool poll(void (*sink)(const Sighting&), uint32_t& seen) override;

 private:
  const bool* enabledFlag_;
  bool ready_ = false;
  bool running_ = false;
  uint8_t channel_ = 11;
  uint32_t hopAt_ = 0;
  uint32_t cycleSeen_ = 0;
  void tune(uint8_t ch);
  void finish();
};
