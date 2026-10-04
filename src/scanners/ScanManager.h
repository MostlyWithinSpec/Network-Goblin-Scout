#pragma once
#include "Scanner.h"

// Round-robins through the registered scanners, one at a time, with a short rest
// between cycles. Non-blocking: call tick() from loop().
class ScanManager {
 public:
  void add(Scanner* s);
  void begin();
  void tick();
  const char* activeName() const;
  bool busy() const { return state_ == State::Running; }

 private:
  enum class State { Idle, Running, Resting };
  static const int kMax = 4;
  Scanner* scanners_[kMax] = {};
  int count_ = 0;
  int current_ = -1;
  State state_ = State::Idle;
  uint32_t restUntil_ = 0;
  void startNext();
};
