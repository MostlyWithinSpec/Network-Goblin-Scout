#pragma once
#include <Arduino.h>
#include <vector>

enum class CState : uint8_t {
  Idle, Scanning, Searching, Discovered, Excited, LevelUp, Achievement,
  Uploading, Sleeping, LowBattery, Offline, SyncDone, COUNT
};
const char* cstateKey(CState s);

struct Frame {
  uint16_t* px = nullptr;  // RGB565, PSRAM
  uint16_t w = 0, h = 0;
};

// A sprite pack from /sprites/<name>/metadata.json + BMP frames (16-bit 565 or 24-bit).
class SpritePack {
 public:
  bool load(const String& name);
  void clear();
  bool loaded() const { return loaded_; }
  const Frame* frame(CState s, uint32_t tick) const;  // nullptr -> draw procedural goblin
  const String& name() const { return name_; }
  const String& author() const { return author_; }

 private:
  std::vector<Frame> frames_[(size_t)CState::COUNT];
  String name_, author_;
  bool loaded_ = false;
};

std::vector<String> listSpritePacks();
