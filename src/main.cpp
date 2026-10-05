// Network Goblin Scout — passive wireless discovery companion
// Target: NM-CYD-C5 (ESP32-C5, 2.8" ST7789 + XPT2046, microSD)

#include <Arduino.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include "board.h"
#include "config.h"
#include "core/Achievements.h"
#include "core/Engine.h"
#include "diag/Diagnostics.h"
#include "hal/Battery.h"
#include "hal/Display.h"
#include "hal/Fx.h"
#include "hal/Gps.h"
#include "hal/Storage.h"
#include "hal/Touch.h"
#include "scanners/BleScanner.h"
#include "scanners/ScanManager.h"
#include "scanners/ThreadScanner.h"
#include "scanners/WifiScanner.h"
#include "social/Peer.h"
#include "ui/Sprites.h"
#include "ui/Ui.h"

namespace {
ScanManager scans;
WifiScanner wifiScanner;
BleScanner bleScanner(&engine.settings().bleScan);
ThreadScanner threadScanner(&engine.settings().scan154);
SpritePack pack;

gfx::Surface* screen = nullptr;
UiModel model;
uint32_t bootMs = 0;
uint32_t lastFrameMs = 0;
uint32_t lastGeoMs = 0;
bool wasTouched = false;
bool btnWasDown = false;
const char* splashLine = "";
uint32_t perfFrames = 0, perfRenderUs = 0, perfFlushUs = 0, perfLogMs = 0;
ui::Profile perfParts = {};

void deselectSpiDevices() {
  // Everything shares one SPI bus: make sure no chip is listening before init.
  for (int pin : {PIN_TFT_CS, PIN_TOUCH_CS, PIN_SD_CS}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
}

void showSplash(const char* line) {
  splashLine = line;
  if (!screen) return;
  ui::splash(*screen, millis() - bootMs, line);
  display::flush();
}
void splashTick() { showSplash(splashLine); }

// ---- UI hooks: the UI asks, main does the hardware part -------------------
const fx::Note kTap[] = {{3200, 12}};
const fx::Note kDiscover[] = {{2400, 30}, {3200, 45}};
const fx::Note kChannel[] = {{2000, 40}, {2600, 40}, {3200, 70}};
const fx::Note kLevelUp[] = {{1047, 90}, {1319, 90}, {1568, 90}, {2093, 260}};
const fx::Note kAchieve[] = {{1568, 80}, {2093, 80}, {2637, 200}};
const fx::Note kEncounter[] = {{1319, 70}, {1760, 70}, {0, 40}, {1319, 70}, {1760, 70}, {2637, 280}};
const fx::Note kPet[] = {{2800, 25}, {3400, 40}};
const fx::Note kQuest[] = {{1760, 60}, {2349, 60}, {2960, 160}};

void onSfx(ui::Sfx s) {
  switch (s) {
    case ui::kSfxTap: fx::play(kTap, 1); break;
    case ui::kSfxDiscover: fx::play(kDiscover, 2); break;
    case ui::kSfxChannel: fx::play(kChannel, 3); break;
    case ui::kSfxLevelUp: fx::play(kLevelUp, 4); break;
    case ui::kSfxAchievement: fx::play(kAchieve, 3); break;
    case ui::kSfxEncounter: fx::play(kEncounter, 6); break;
    case ui::kSfxPet: fx::play(kPet, 2); break;
    case ui::kSfxQuest: fx::play(kQuest, 3); break;
    case ui::kSfxBabble: {  // goblin "talking": a few random chirps, Animal Crossing style
      fx::Note n[5];
      for (auto& note : n) note = {(uint16_t)(1400 + esp_random() % 1400), (uint16_t)(35 + esp_random() % 25)};
      fx::play(n, 5);
      break;
    }
  }
}

void onBeacon(bool on) {
  if (on) peer::startBeacon(engine.level());
  else peer::stopBeacon();
}

void onGps(bool on) {
  if (on) gps::begin();
}

bool nextPack() {
  auto packs = listSpritePacks();
  if (packs.empty()) return false;
  Settings& st = engine.settings();
  size_t idx = 0;
  for (size_t i = 0; i < packs.size(); i++)
    if (packs[i] == st.spritePack) idx = (i + 1) % packs.size();
  st.spritePack = packs[idx];
  if (!pack.load(st.spritePack)) ui::notice("Pack failed, using goblin", millis());
  engine.settingsChanged();
  return true;
}

const Frame* packFrame(CState s, uint32_t tick) { return pack.frame(s, tick); }

ui::Hooks makeHooks() {
  ui::Hooks h;
  h.brightness = display::setBrightness;
  h.sound = fx::setSound;
  h.invert = display::setInverted;
  h.fastDisplay = display::setFast;
  h.beacon = onBeacon;
  h.gps = onGps;
  h.nextPack = nextPack;
  h.settingsChanged = [] { engine.settingsChanged(); };
  h.pet = [] { engine.pet(); };
  h.hat = peer::setHat;
  h.agreed = [] {
    engine.settings().agreed = true;
    peer::setAgreed();
    engine.settingsChanged();
  };
  h.named = [](const char* name) {
    peer::setName(name);
    engine.settings().goblinName = peer::self().name;
    engine.settingsChanged();
  };
  h.sfx = onSfx;
  h.led = fx::led;
  h.micros = [] { return (uint32_t)::micros(); };
  return h;
}

// ---- Per-frame model ------------------------------------------------------
void fillModel(uint32_t now) {
  Stats& st = engine.stats();
  model.now = now;
  model.stats = &st;
  model.settings = &engine.settings();
  model.level = engine.level();
  model.xpLo = progression::xpForLevel(model.level);
  model.xpHi = progression::xpForLevel(model.level + 1);
  model.sd = storage::ok();
  model.gpsPresent = engine.settings().gps && gps::present();
  model.gpsFix = model.gpsPresent && gps::hasFix();
  model.sats = gps::satellites();
  model.scanning = scans.activeName();
  model.beaconOn = peer::beaconOn();
  model.battPresent = battery::present();
  model.battPct = battery::percent();
  model.battCharging = battery::charging();
  model.myName = peer::self().name;
  model.myHue = peer::self().hue;
  model.myId = peer::self().id;
  model.nearbyCount = (uint8_t)engine.nearbyPeers(PEER_TOGETHER_MS, model.nearby, 6);
  model.packFrame = pack.loaded() ? packFrame : nullptr;
  model.packName = pack.loaded() ? engine.settings().spritePack.c_str() : "built-in";
  touch::lastRaw(model.rawX, model.rawY, model.rawZ);
  model.fwVersion = NG_FW_VERSION;
  model.mood = engine.mood();
  model.hunger = (uint16_t)st.hunger;
  model.boredom = (uint16_t)st.boredom;
  model.quests = &engine.quests();
  model.uptimeMin = st.uptimeMin;
  model.hatMask = engine.hatMask();
  model.blips = engine.blips(model.blipCount);
  model.shareUrl = NG_SHARE_URL;
}

void handleEvents(uint32_t now) {
  UiEvent e;
  while (engine.popEvent(e)) {
    log_i("event: %s", e.text);
    ui::onEvent(e, now);
    if (e.type == EventType::LevelUp) peer::update((uint16_t)e.value);
  }
}

void handleInput() {
  int16_t x = 0, y = 0;
  bool down = touch::read(x, y);
  if (display::asleep()) {
    if (down && !wasTouched) display::sleep(false);  // a tap only wakes the screen
  } else {
    ui::onTouch(down, x, y, model);
  }
  wasTouched = down;

  // BOOT button toggles "pocket mode": screen off, scanning continues.
  bool btn = digitalRead(PIN_BOOT_BTN) == LOW;
  if (btn && !btnWasDown) display::sleep(!display::asleep());
  btnWasDown = btn;
}
}  // namespace

void setup() {
  bootMs = millis();
  Serial.begin(115200);
  delay(200);
  log_i("NG Scout %s booting, PSRAM %u KB free", NG_FW_VERSION, ESP.getFreePsram() / 1024);

  deselectSpiDevices();
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

  if (display::begin() && display::framebuffer())
    screen = new gfx::Surface(display::framebuffer(), SCREEN_W, SCREEN_H);
  else
    log_e("display init failed");
  // Second full-screen buffer in PSRAM for the pre-rendered background (optional).
  auto* bg = (uint16_t*)heap_caps_malloc(SCREEN_W * SCREEN_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  ui::begin(makeHooks(), bg);
  showSplash("waking the goblin...");

  touch::begin();
  storage::begin();

  // Bring-up mode. Holding BOOT *while* powering on may put the ESP32-C5 into its
  // USB flashing mode instead of running us (BOOT is a strapping pin), so we give
  // a short window after power-on to press it instead.
  splashLine = "press BOOT now for diagnostics";
  if (diag::requested(1500, splashTick)) {
    Scanner* const radios[] = {&wifiScanner, &bleScanner, &threadScanner};
    diag::run(radios, sizeof(radios) / sizeof(radios[0]));
  }

  battery::begin();
  showSplash("opening the hoard...");
  engine.begin();

  Settings& st = engine.settings();
  // Identity: the SD copy wins (a factory flash wipes NVS), then both copies are synced.
  peer::begin(st.goblinId, st.goblinName.c_str());
  bool agreed = st.agreed || peer::agreedNvs();
  if (st.goblinId != peer::self().id || (peer::named() && st.goblinName != peer::self().name) || agreed != st.agreed) {
    st.goblinId = peer::self().id;
    if (peer::named()) st.goblinName = peer::self().name;
    st.agreed = agreed;
    engine.settingsChanged();
  }

  display::setBrightness(st.brightness);
  display::setInverted(st.invert);
  display::setFast(st.fastDisplay);
  fx::begin();
  fx::setSound(st.sound);
  if (st.gps) gps::begin();
  if (!pack.load(st.spritePack)) log_i("no sprite pack, using built-in goblin");

  // First run: disclaimer, then name the goblin. Nothing is scanned until this is done.
  if (screen && (!agreed || !peer::named())) {
    ui::startOnboarding(!agreed, !peer::named());
    uint32_t last = 0;
    while (ui::onboarding()) {
      uint32_t now = millis();
      int16_t x = 0, y = 0;
      bool down = touch::read(x, y);
      fillModel(now);
      ui::onTouch(down, x, y, model);
      if (now - last >= UI_FRAME_MS) {
        last = now;
        ui::render(*screen, model);
        display::flush();
      }
      fx::update();
      delay(1);
    }
  }

  showSplash("warming up radios...");
  scans.add(&wifiScanner);
  scans.add(&bleScanner);
  scans.add(&threadScanner);
  scans.begin();

  // Goblin identity + "I'm a goblin" beacon (needs BLE, which scans.begin() started).
  peer::setHat(st.hat);
  if (st.beacon) peer::startBeacon(engine.level());

  if (!storage::ok()) ui::notice("No SD card: progress won't save", millis());
  fillModel(millis());
  log_i("setup done, heap %u KB, PSRAM %u KB free", ESP.getFreeHeap() / 1024, ESP.getFreePsram() / 1024);
}

void loop() {
  uint32_t now = millis();

  if (engine.settings().gps) gps::poll();
  battery::poll();
  engine.setOnBattery(battery::discharging());
  scans.tick();

  if (now - lastGeoMs > 1000) {
    lastGeoMs = now;
    engine.updateLocation();
  }

  handleEvents(now);
  handleInput();
  fx::update();

  ui::pet().setBase(display::asleep() ? CState::Sleeping : scans.busy() ? CState::Scanning : CState::Idle);

  if (now - lastFrameMs >= UI_FRAME_MS) {
    lastFrameMs = now;
    fillModel(now);
    if (!display::asleep() && screen) {
      uint32_t t0 = micros();
      ui::render(*screen, model);
      uint32_t t1 = micros();
      display::flush();
      perfRenderUs += t1 - t0;
      perfFlushUs += micros() - t1;
      perfFrames++;
      const ui::Profile& p = ui::profile();
      perfParts.background += p.background;
      perfParts.screen += p.screen;
      perfParts.chrome += p.chrome;
      perfParts.overlays += p.overlays;
    }
    if (now - perfLogMs >= 10000) {  // frame timing, for tuning on real hardware
      if (perfFrames) {
        uint32_t n = perfFrames * 1000;  // -> ms
        log_i("ui: %lu fps, render %lu ms (bg %lu, screen %lu, bars %lu, overlays %lu), screen push %lu ms",
              (unsigned long)(perfFrames * 1000 / (now - perfLogMs)), (unsigned long)(perfRenderUs / n),
              (unsigned long)(perfParts.background / n), (unsigned long)(perfParts.screen / n),
              (unsigned long)(perfParts.chrome / n), (unsigned long)(perfParts.overlays / n),
              (unsigned long)(perfFlushUs / n));
      }
      perfLogMs = now;
      perfFrames = perfRenderUs = perfFlushUs = 0;
      perfParts = {};
    }
  }

  engine.tick();
  delay(1);
}
