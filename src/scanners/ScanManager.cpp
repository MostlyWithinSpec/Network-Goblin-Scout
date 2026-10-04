#include "ScanManager.h"
#include "../core/Engine.h"
#include "config.h"

namespace {
void sink(const Sighting& s) { engine.process(s); }
}  // namespace

void ScanManager::add(Scanner* s) {
  if (count_ < kMax) scanners_[count_++] = s;
}

void ScanManager::begin() {
  for (int i = 0; i < count_; i++) {
    bool ok = scanners_[i]->begin();
    log_i("scan: %s %s", scanners_[i]->name(), ok ? "ready" : "FAILED");
  }
  state_ = State::Idle;
}

void ScanManager::startNext() {
  for (int tries = 0; tries < count_; tries++) {
    current_ = (current_ + 1) % count_;
    if (scanners_[current_]->enabled()) {
      scanners_[current_]->start();
      state_ = State::Running;
      return;
    }
  }
  state_ = State::Idle;  // nothing enabled
}

void ScanManager::tick() {
  switch (state_) {
    case State::Idle:
      startNext();
      break;
    case State::Running: {
      uint32_t seen = 0;
      Scanner* s = scanners_[current_];
      if (s->poll(sink, seen)) {
        engine.endScan(s->radio(), seen);
        // Rest after the last scanner in the round, otherwise go straight on.
        bool endOfRound = true;
        for (int i = current_ + 1; i < count_; i++)
          if (scanners_[i]->enabled()) { endOfRound = false; break; }
        if (endOfRound) {
          state_ = State::Resting;
          restUntil_ = millis() + SCAN_REST_MS;
        } else {
          startNext();
        }
      }
      break;
    }
    case State::Resting:
      if ((int32_t)(millis() - restUntil_) >= 0) startNext();
      break;
  }
}

const char* ScanManager::activeName() const {
  if (state_ != State::Running || current_ < 0) return nullptr;
  return scanners_[current_]->name();
}
