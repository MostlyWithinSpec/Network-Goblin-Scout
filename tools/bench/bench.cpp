// Renders UI frames for the ESP32-C5's CPU (rv32imac, no FPU) so tools/bench/run.py can
// count instructions in an emulator. bench_mark() calls delimit the measured sections.
#include <cstring>
#include "core/Achievements.h"
#include "ui/Ui.h"

static uint16_t fb[320 * 240], bg[320 * 240];
static Stats stats;
static Settings settings;
static UiModel model;
static Blip blips[48];
static QuestBoard board;

extern "C" __attribute__((noinline)) void bench_mark(int id) { asm volatile("" ::"r"(id)); }
extern "C" __attribute__((noinline)) void bench_done() { asm volatile(""); }

static UiEvent ev(EventType t, uint32_t v) {
  UiEvent e{};
  e.type = t;
  e.value = v;
  strcpy(e.text, "x");
  return e;
}

int main() {
  gfx::Surface surf(fb, 320, 240);
  stats.xp = 5230; stats.wifiUnique = 1342; stats.bleUnique = 412; stats.t154Unique = 37; stats.peersMet = 3;
  stats.sessWifiNew = 23; stats.sessXp = 156;
  model.stats = &stats; model.settings = &settings; model.level = 11; model.xpLo = 5000; model.xpHi = 6050;
  model.myName = "Grimhex"; model.sd = true; model.beaconOn = true;
  uint32_t seed = 99;
  for (auto& b : blips) {
    seed = seed * 1103515245 + 12345;
    b.angle = (seed >> 8) & 4095;
    b.rssi = (int8_t)(-35 - (int)((seed >> 20) % 60));
    b.radio = (seed >> 4) % 10 < 6 ? Radio::WiFi : Radio::BLE;
    b.lastSeenMs = 1000;
  }
  model.blips = blips;
  model.blipCount = 48;
  model.quests = &board;
  settings.hat = 8;
  ui::begin(ui::Hooks(), bg);
  uint32_t now = 1000;
  model.now = now; ui::render(surf, model);  // warm-up: builds the static background
  bench_mark(1);  // home idle, 3 frames
  for (int i = 0; i < 3; i++) { now += 40; model.now = now; ui::render(surf, model); }
  bench_mark(2);  // home while scanning, 3 frames
  model.scanning = "Wi-Fi";
  ui::pet().setBase(CState::Scanning);
  for (int i = 0; i < 3; i++) { now += 40; model.now = now; ui::render(surf, model); }
  bench_mark(3);  // level-up overlay, 3 frames
  ui::onEvent(ev(EventType::LevelUp, 12), now);
  now += 1200;
  for (int i = 0; i < 3; i++) { now += 40; model.now = now; ui::render(surf, model); }
  now += 5000;
  bench_mark(4);  // radar, 3 frames
  ui::debugShow(1, 0);
  for (auto& b : blips) b.lastSeenMs = now;
  for (int i = 0; i < 3; i++) { now += 40; model.now = now; ui::render(surf, model); }
  bench_mark(5);
  bench_done();
  return 0;
}
