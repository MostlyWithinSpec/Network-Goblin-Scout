// Network Goblin Scout — passive wireless discovery companion
// Target: NM-CYD-C5 (ESP32-C5, 2.8" ST7789 + XPT2046, microSD)

#include <Arduino.h>
#include <SPI.h>
#include "board.h"
#include "config.h"
#include "core/Engine.h"
#include "hal/Display.h"
#include "hal/Fx.h"
#include "hal/Gps.h"
#include "hal/Storage.h"
#include "hal/Touch.h"
#include "scanners/BleScanner.h"
#include "scanners/ScanManager.h"
#include "scanners/ThreadScanner.h"
#include "scanners/WifiScanner.h"
#include "ui/Companion.h"
#include "ui/Ui.h"

namespace {
Companion companion;
ScanManager scans;
WifiScanner wifiScanner;
BleScanner bleScanner(&engine.settings().bleScan);
ThreadScanner threadScanner;

uint32_t lastFrameMs = 0;
uint32_t lastGeoMs = 0;
bool wasTouched = false;
bool btnWasDown = false;

void deselectSpiDevices() {
  // Everything shares one SPI bus: make sure no chip is listening before init.
  for (int pin : {PIN_TFT_CS, PIN_TOUCH_CS, PIN_SD_CS}) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }
}

void handleEvents() {
  UiEvent e;
  while (engine.popEvent(e)) {
    switch (e.type) {
      case EventType::NewWifi:
      case EventType::NewBle:
        companion.react(CState::Discovered, 1500);
        ui::toast(e.text, 0x5E8A);
        fx::led(0, 40, 0, 120);
        fx::chirp(2400, 30);
        break;
      case EventType::NewChannel:
      case EventType::NewCell:
      case EventType::DailyBonus:
        companion.react(CState::Excited, 2000);
        ui::toast(e.text, 0x2D7F);
        fx::led(0, 20, 60, 200);
        fx::chirp(2800, 60);
        break;
      case EventType::LevelUp:
        companion.react(CState::LevelUp, 3500);
        ui::toast(e.text, 0xFD20);
        fx::led(60, 30, 0, 600);
        fx::chirp(3500, 250);
        break;
      case EventType::Achievement:
        companion.react(CState::Achievement, 3000);
        ui::toast(e.text, 0xFFE0);
        fx::led(50, 50, 0, 400);
        fx::chirp(3000, 150);
        break;
    }
    log_i("event: %s", e.text);
  }
}

void handleInput() {
  // Touch: act on press edge only
  int16_t x, y;
  bool down = touch::read(x, y);
  if (down && !wasTouched) {
    if (display::asleep()) {
      display::sleep(false);
    } else {
      ui::onTap(x, y);
    }
  }
  wasTouched = down;

  // BOOT button toggles "pocket mode": screen off, scanning continues.
  bool btn = digitalRead(PIN_BOOT_BTN) == LOW;
  if (btn && !btnWasDown) display::sleep(!display::asleep());
  btnWasDown = btn;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  log_i("NG Scout %s booting, PSRAM %u KB free", NG_FW_VERSION, ESP.getFreePsram() / 1024);

  deselectSpiDevices();
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

  if (!display::begin()) log_e("display init failed");
  ui::begin(&companion, &scans);
  ui::splash("waking the goblin...");

  touch::begin();
  storage::begin();
  engine.begin();

  const Settings& st = engine.settings();
  display::setBrightness(st.brightness);
  display::setInverted(st.invert);
  fx::begin();
  fx::setSound(st.sound);
  if (st.gps) gps::begin();
  if (!companion.pack.load(st.spritePack)) log_i("no sprite pack, using built-in goblin");

  ui::splash("warming up radios...");
  scans.add(&wifiScanner);
  scans.add(&bleScanner);
  scans.add(&threadScanner);  // disabled stub for v1.1
  scans.begin();

  if (!storage::ok()) ui::toast("No SD: progress won't save", 0xF800);
  log_i("setup done, heap %u KB, PSRAM %u KB free", ESP.getFreeHeap() / 1024, ESP.getFreePsram() / 1024);
}

void loop() {
  uint32_t now = millis();

  if (engine.settings().gps) gps::poll();
  scans.tick();

  if (now - lastGeoMs > 1000) {
    lastGeoMs = now;
    engine.updateLocation();
  }

  handleEvents();
  handleInput();
  fx::update();

  companion.setBase(display::asleep() ? CState::Sleeping
                    : scans.busy()    ? CState::Scanning
                                      : CState::Idle);

  if (now - lastFrameMs >= UI_FRAME_MS) {
    lastFrameMs = now;
    if (!display::asleep()) ui::draw();
  }

  engine.tick();
  delay(2);
}
