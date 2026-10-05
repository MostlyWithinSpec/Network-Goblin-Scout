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
  bool battPresent = false;          // MAX17048 fuel gauge fitted
  uint8_t battPct = 0;
  bool battCharging = false;

  // this goblin + others nearby
  const char* myName = "Goblin";
  uint8_t myHue = 90;
  uint32_t myId = 0;
  const NearbyPeer* nearby[6] = {};
  uint8_t nearbyCount = 0;
  uint8_t trackersNearby = 0;        // item trackers heard in the last 2 minutes

  // optional SD sprite pack frame for the current pet state (null = built-in goblin)
  const Frame* (*packFrame)(CState s, uint32_t tick) = nullptr;
  const char* packName = "goblin";

  // needs, quests, cosmetics
  Mood mood = Mood::Content;
  uint16_t hunger = 0, boredom = 0;   // 0..1000
  const QuestBoard* quests = nullptr;
  uint32_t uptimeMin = 0;             // for "new quests in N min"
  uint32_t hatMask = 0;               // unlocked hats, bit (id - 1)

  // radar
  const Blip* blips = nullptr;
  size_t blipCount = 0;

  // wall clock (core/Clock.h): local unix seconds, 0 = unknown
  uint32_t localTime = 0;
  bool clockGps = false;              // from GPS (else set by hand)
  uint32_t buildTime = 0;             // firmware build time: starting point for setting the clock

  // share card QR code target
  const char* shareUrl = "";

  // touch test
  uint16_t rawX = 0, rawY = 0, rawZ = 0;
  const char* fwVersion = "";
};
