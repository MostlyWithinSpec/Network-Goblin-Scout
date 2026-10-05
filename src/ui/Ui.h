#pragma once
#include "Companion.h"
#include "Model.h"
#include "gfx/Surface.h"

// Screens, overlays, animation and touch handling. Pure: draws from a UiModel and asks
// main.cpp to do anything hardware-related through Hooks (so it also runs on a PC).
namespace ui {

enum Sfx : uint8_t {
  kSfxTap, kSfxDiscover, kSfxChannel, kSfxLevelUp, kSfxAchievement, kSfxEncounter, kSfxPet, kSfxBabble, kSfxQuest
};

struct Hooks {
  void (*brightness)(uint8_t pct) = nullptr;
  void (*sound)(bool on) = nullptr;
  void (*invert)(bool on) = nullptr;
  void (*fastDisplay)(bool on) = nullptr;
  void (*beacon)(bool on) = nullptr;
  void (*gps)(bool on) = nullptr;
  bool (*nextPack)() = nullptr;          // cycle SD sprite packs; false if there are none
  void (*settingsChanged)() = nullptr;   // persist settings
  void (*pet)() = nullptr;               // goblin was petted
  void (*hat)(uint8_t id) = nullptr;     // hat equipped (0 = none)
  void (*sfx)(Sfx s) = nullptr;
  void (*led)(uint8_t r, uint8_t g, uint8_t b, uint16_t ms) = nullptr;
  uint32_t (*micros)() = nullptr;        // optional: enables profile()
  void (*agreed)() = nullptr;            // first-run disclaimer accepted
  void (*named)(const char* name) = nullptr;  // goblin (re)named
};

// Time spent in each part of the last render(), in microseconds (needs Hooks::micros).
struct Profile {
  uint32_t background, screen, chrome, overlays;
};
const Profile& profile();

// bgBuffer: optional 320x240 RGB565 scratch buffer for the pre-rendered background
// (saves redrawing it every frame). May be null.
void begin(const Hooks& hooks, uint16_t* bgBuffer);
void onEvent(const UiEvent& e, uint32_t now);
void notice(const char* text, uint32_t now);  // red system banner, e.g. "No SD card"
void onTouch(bool down, int16_t x, int16_t y, const UiModel& m);
void render(gfx::Surface& s, const UiModel& m);
void splash(gfx::Surface& s, uint32_t now, const char* line);
Companion& pet();

// First-run flow: disclaimer and/or naming, shown full-screen before the normal tabs.
// main.cpp keeps rendering and feeding touches until onboarding() returns false.
void startOnboarding(bool disclaimer, bool name);
bool onboarding();
bool animating();  // an overlay or transition is running (main can skip frame limiting)

// For the preview tool.
void debugShow(int screen, int page);
void debugBadge(int index);  // open the detail card (-1 = close)
}  // namespace ui
