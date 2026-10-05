#pragma once
#include <Arduino.h>
#include <vector>
#include "Companion.h"

const char* cstateKey(CState s);

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
