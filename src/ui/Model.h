#pragma once
#include "../core/Types.h"
#include "Companion.h"

// Everything the UI needs to draw one frame, gathered by main.cpp from the engine and
// hardware. The UI never calls hardware directly, so it also runs in the PC preview tool.
struct UiModel {
  uint32_t now = 0;              // ms
  Stats* stats = nullptr;
  Settings* settings = nullptr;
  uint16_t level = 1;
  uint32_t xpLo = 0, xpHi = 50;  // XP at the start of this level and of the next

  // status bar
  bool sd = false;
  bool gpsPresent = false, gpsFix = false;
  uint8_t sats = 0;
  const char* scanning = nullptr;  // name of the radio scanning right now, or null
  bool beaconOn = false;

  // this goblin + others nearby
  const char* myName = "Goblin";
  uint8_t myHue = 90;
  uint32_t myId = 0;
  const NearbyPeer* nearby[6] = {};
  uint8_t nearbyCount = 0;

  // optional SD sprite pack frame for the current pet state (null = built-in goblin)
  const Frame* (*packFrame)(CState s, uint32_t tick) = nullptr;
  const char* packName = "goblin";

  // touch test
  uint16_t rawX = 0, rawY = 0, rawZ = 0;
  const char* fwVersion = "";
};
