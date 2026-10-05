#include "Diagnostics.h"
#include <SD.h>
#include <Wire.h>
#include <esp_memory_utils.h>
#include <esp_random.h>
#include <time.h>
#include "../hal/Aht20.h"
#include "../hal/Battery.h"
#include "../hal/Display.h"
#include "../hal/Fx.h"
#include "../hal/Gps.h"
#include "../hal/Storage.h"
#include "../hal/Touch.h"
#include "board.h"
#include "config.h"

namespace {
const uint16_t BG = 0x0000;
const uint16_t TEXT = 0xFFFF;
const uint16_t DIM = 0x8C71;
const uint16_t GOOD = 0x07E0;
const uint16_t BAD = 0xF800;
const uint16_t WARN = 0xFD20;

// ---- Radios ---------------------------------------------------------------
struct RadioDiag {
  bool ready = false;
  uint32_t cycles = 0;
  // Wi-Fi: a 2.4 GHz, b 5 GHz.  BLE: a stable, b rotating, c goblins.
  // 802.15.4: a Zigbee, b Thread, c unknown (frames, not devices).
  uint32_t a = 0, b = 0, c = 0;
  int8_t best = -127;
};
const size_t kMaxRadios = 4;
Scanner* const* scanners = nullptr;
size_t scannerCount = 0;
RadioDiag radios[kMaxRadios];
int current = -1;
bool running = false;
uint32_t cycA = 0, cycB = 0, cycC = 0;
int8_t cycBest = -127;

void sink(const Sighting& s) {
  switch (s.radio) {
    case Radio::WiFi: (s.channel <= 14 ? cycA : cycB)++; break;
    case Radio::BLE: (s.stableAddr ? cycA : cycB)++; break;
    case Radio::Peer: cycC++; break;
    case Radio::Thread:
      (s.flags & sflag::kZigbee ? cycA : s.flags & sflag::kThread ? cycB : cycC)++;
      break;
  }
  if (s.rssi > cycBest) cycBest = s.rssi;
}

// Same round-robin as ScanManager (the radios share one 2.4 GHz front end).
void tickScanners() {
  if (!running) {
    for (size_t tries = 0; tries < scannerCount; tries++) {
      current = (current + 1) % (int)scannerCount;
      if (!radios[current].ready) continue;
      cycA = cycB = cycC = 0;
      cycBest = -127;
      scanners[current]->start();
      running = true;
      return;
    }
    return;
  }
  uint32_t seen = 0;
  if (scanners[current]->poll(sink, seen)) {
    RadioDiag& r = radios[current];
    r.cycles++;
    r.a = cycA;
    r.b = cycB;
    r.c = cycC;
    r.best = cycBest;
    running = false;
  }
}

// ---- SD -------------------------------------------------------------------
struct {
  bool mounted = false;
  uint8_t type = CARD_NONE;
  uint64_t sizeMB = 0;
  bool rwOk = false;
} sd;

void checkSd() {
  sd.mounted = storage::ok();
  if (!sd.mounted) return;
  sd.type = SD.cardType();
  sd.sizeMB = SD.cardSize() >> 20;
  const char* path = "/ng_diag.tmp";
  uint8_t out[64], in[64] = {0};
  esp_fill_random(out, sizeof(out));
  sd.rwOk = storage::writeBytes(path, out, sizeof(out)) && storage::readBytes(path, in, sizeof(in)) &&
            memcmp(out, in, sizeof(out)) == 0;
  SD.remove(path);
}

const char* cardTypeName(uint8_t t) {
  switch (t) {
    case CARD_MMC: return "MMC";
    case CARD_SD: return "SDSC";
    case CARD_SDHC: return "SDHC/XC";
    default: return "unknown";
  }
}

// ---- I2C / AHT20 ----------------------------------------------------------
struct {
  int sda = PIN_I2C_SDA, scl = PIN_I2C_SCL;
  uint8_t addrs[12];
  uint8_t n = 0;
  bool measuring = false;
  bool valid = false;
  float tempC = 0, rh = 0;
  uint32_t readAt = 0;
} env;

void scanBus(int sda, int scl) {
  Wire.end();
  Wire.begin(sda, scl, 100000);
  env.sda = sda;
  env.scl = scl;
  env.n = 0;
  for (uint8_t a = 1; a < 127 && env.n < sizeof(env.addrs); a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) env.addrs[env.n++] = a;
  }
}

void checkI2c() {
  // The schematic says SDA 9 / SCL 8; the vendor's Bruce config lists them the other
  // way round. Try the schematic first, then swapped, and report which one answered.
  const int orders[2][2] = {{PIN_I2C_SDA, PIN_I2C_SCL}, {PIN_I2C_SCL, PIN_I2C_SDA}};
  bool found = false;
  for (const auto& o : orders) {
    scanBus(o[0], o[1]);
    if ((found = aht20::begin(Wire))) break;
  }
  if (!found) scanBus(PIN_I2C_SDA, PIN_I2C_SCL);  // neither worked: report the schematic order
  battery::begin();  // optional MAX17048 fuel gauge (0x36), uses the bus as set up above
}

void tickAht(uint32_t now) {
  if (!aht20::present()) return;
  if (!env.measuring && (int32_t)(now - env.readAt) >= 0) {
    env.measuring = aht20::start();
    env.readAt = now + 100;  // datasheet: measurement takes ~80 ms
  } else if (env.measuring && (int32_t)(now - env.readAt) >= 0) {
    env.valid = aht20::read(env.tempC, env.rh);
    env.measuring = false;
    env.readAt = now + 2000;
  }
}

// ---- Touch / LED ----------------------------------------------------------
int16_t tapX = -1, tapY = -1;
uint32_t taps = 0;
bool wasDown = false;

void tickTouch() {
  int16_t x, y;
  bool down = touch::read(x, y);
  if (down && !wasDown) {
    taps++;
    fx::chirp(2400, 40);
  }
  if (down) { tapX = x; tapY = y; }
  wasDown = down;
}

const char* kLedNames[] = {"RED", "GREEN", "BLUE"};
int ledIdx = -1;

void tickLed(uint32_t now) {
  int idx = (now / 1000) % 3;
  if (idx == ledIdx) return;
  ledIdx = idx;
  fx::led(idx == 0 ? 40 : 0, idx == 1 ? 40 : 0, idx == 2 ? 40 : 0, 1100);
}

// ---- Report (shared by screen and serial) ---------------------------------
struct Line {
  char text[60];
  uint16_t color;
};
Line lines[20];
size_t nLines = 0;

void add(uint16_t color, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void add(uint16_t color, const char* fmt, ...) {
  if (nLines >= sizeof(lines) / sizeof(lines[0])) return;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(lines[nLines].text, sizeof(lines[nLines].text), fmt, ap);
  va_end(ap);
  lines[nLines++].color = color;
}

void addRadio(size_t i) {
  const RadioDiag& r = radios[i];
  Radio radio = scanners[i]->radio();
  const char* name = radio == Radio::WiFi ? "WIFI " : radio == Radio::BLE ? "BLE  " : "15.4 ";
  uint32_t total = r.a + r.b + r.c;
  if (!r.ready) { add(BAD, "%s init FAILED", name); add(DIM, " "); return; }
  if (!r.cycles) { add(WARN, "%s first scan running...", name); add(DIM, " "); return; }
  // No 802.15.4 traffic is normal in a home without Zigbee/Thread gear, so it's not a warning.
  add(total ? GOOD : radio == Radio::Thread ? TEXT : WARN, "%s scans %lu  last %lu seen  best %d dBm", name,
      (unsigned long)r.cycles, (unsigned long)total, total ? r.best : 0);
  if (radio == Radio::WiFi)
    add(TEXT, "      2.4 GHz %lu   5 GHz %lu", (unsigned long)r.a, (unsigned long)r.b);
  else if (radio == Radio::BLE)
    add(TEXT, "      stable %lu  rotating %lu  goblins %lu", (unsigned long)r.a, (unsigned long)r.b,
        (unsigned long)r.c);
  else
    add(TEXT, "      frames: zigbee %lu thread %lu other %lu", (unsigned long)r.a, (unsigned long)r.b,
        (unsigned long)r.c);
}

void build() {
  nLines = 0;
  uint32_t now = millis();
  add(WARN, "NG SCOUT BRING-UP  fw %s  up %lus", NG_FW_VERSION, (unsigned long)(now / 1000));
  add(TEXT, "CHIP  %s rev %u  %lu MHz  flash %lu MB", ESP.getChipModel(), (unsigned)ESP.getChipRevision(),
      (unsigned long)ESP.getCpuFreqMHz(), (unsigned long)(ESP.getFlashChipSize() >> 20));

  uint32_t psram = ESP.getPsramSize();
  if (psram)
    add(GOOD, "PSRAM %lu KB, %lu KB free", (unsigned long)(psram >> 10), (unsigned long)(ESP.getFreePsram() >> 10));
  else
    add(BAD, "PSRAM not found");

  Arduino_Canvas* c = display::gfx();
  void* fb = c ? c->getFramebuffer() : nullptr;
  add(fb ? TEXT : BAD, "HEAP  %lu KB free  screen buf: %s", (unsigned long)(ESP.getFreeHeap() >> 10),
      !fb ? "NONE" : esp_ptr_external_ram(fb) ? "PSRAM" : "internal");

  if (!sd.mounted)
    add(BAD, "SD    no card / mount failed");
  else
    add(sd.rwOk ? GOOD : BAD, "SD    %s %llu MB  write test %s", cardTypeName(sd.type),
        (unsigned long long)sd.sizeMB, sd.rwOk ? "PASS" : "FAIL");

  uint16_t rx, ry, rz;
  touch::lastRaw(rx, ry, rz);
  bool irq = digitalRead(PIN_TOUCH_IRQ) == LOW;
  add(irq ? GOOD : TEXT, "TOUCH irq %s  raw x %u  y %u  z %u", irq ? "LOW" : "high", rx, ry, rz);
  add(TEXT, "      mapped %d,%d  taps %lu", tapX, tapY, (unsigned long)taps);

  for (size_t i = 0; i < scannerCount; i++) addRadio(i);

  uint32_t chars = gps::charsReceived();
  if (!chars) {
    add(DIM, "GPS   no data on RX %d @ %d baud", PIN_GPS_RX, GPS_BAUD);
    add(DIM, " ");
  } else {
    add(gps::sentencesOk() ? GOOD : BAD, "GPS   %lu bytes  NMEA ok %lu  bad %lu", (unsigned long)chars,
        (unsigned long)gps::sentencesOk(), (unsigned long)gps::sentencesFailed());
    char when[24] = "no time yet";
    if (gps::timeValid()) {
      time_t t = gps::unixTime();
      struct tm tm;
      gmtime_r(&t, &tm);
      strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%SZ", &tm);
    }
    // Fix status only: coordinates never leave the SD card.
    add(TEXT, "      fix %s  sats %u  %s", gps::hasFix() ? "YES" : "no", gps::satellites(), when);
  }

  if (aht20::present()) {
    if (env.valid)
      add(GOOD, "AHT20 %.1f C  %.1f %%RH  (SDA %d, SCL %d)", env.tempC, env.rh, env.sda, env.scl);
    else
      add(WARN, "AHT20 answers at 0x38, no valid reading yet (SDA %d SCL %d)", env.sda, env.scl);
  } else {
    // Nothing at 0x38 on either pin order: the part is most likely not fitted.
    add(BAD, "AHT20 no reply at 0x38 (tried SDA/SCL %d/%d + %d/%d)", PIN_I2C_SDA, PIN_I2C_SCL, PIN_I2C_SCL,
        PIN_I2C_SDA);
  }

  if (battery::present())
    add(battery::percent() > 15 ? GOOD : WARN, "BATT  %u%%  %u mV  %s", battery::percent(), battery::millivolts(),
        battery::charging() ? "charging" : battery::discharging() ? "on battery" : "resting");
  else
    add(DIM, "BATT  no MAX17048 fuel gauge (optional, see docs/battery.md)");

  char devs[40] = "none";
  size_t used = 0;
  for (uint8_t i = 0; i < env.n && used < sizeof(devs) - 6; i++)
    used += snprintf(devs + used, sizeof(devs) - used, "%s0x%02X", i ? " " : "", env.addrs[i]);
  add(TEXT, "I2C   devices: %s", devs);

  add(TEXT, "BOOT  %s     LED should be %s", digitalRead(PIN_BOOT_BTN) == LOW ? "PRESSED" : "up",
      ledIdx < 0 ? "-" : kLedNames[ledIdx]);
}

void draw() {
  Arduino_Canvas* g = display::gfx();
  if (!g || !g->getFramebuffer()) return;
  g->fillScreen(BG);
  g->setTextSize(1);
  int16_t y = 2;
  for (size_t i = 0; i < nLines; i++, y += 11) {
    g->setTextColor(lines[i].color);
    g->setCursor(2, y);
    g->print(lines[i].text);
  }
  g->setTextColor(DIM);
  g->setCursor(2, SCREEN_H - 10);
  g->print("Tap: crosshair + beep.  Press RESET to exit.");

  // Colour swatches: if a label doesn't match its colour, the panel's RGB/BGR
  // order or inversion is wrong.
  const struct { uint16_t c; const char* n; } sw[] = {
      {0xF800, "RED"}, {0x07E0, "GRN"}, {0x001F, "BLU"}, {0xFFFF, "WHT"}};
  for (int i = 0; i < 4; i++) {
    int16_t sy = 60 + i * 30;
    g->fillRect(SCREEN_W - 34, sy, 32, 26, sw[i].c);
    g->setTextColor(i == 3 ? 0x0000 : 0xFFFF);
    g->setCursor(SCREEN_W - 27, sy + 9);
    g->print(sw[i].n);
  }
  // Corner marks: all four should be visible and touching the screen edges.
  g->fillRect(0, 0, 6, 2, WARN);
  g->fillRect(SCREEN_W - 6, 0, 6, 2, WARN);
  g->fillRect(0, SCREEN_H - 2, 6, 2, WARN);
  g->fillRect(SCREEN_W - 6, SCREEN_H - 2, 6, 2, WARN);

  if (tapX >= 0) {
    g->drawFastHLine(tapX - 10, tapY, 21, BAD);
    g->drawFastVLine(tapX, tapY - 10, 21, BAD);
  }
  display::flush();
}

void printSerial() {
  for (size_t i = 0; i < nLines; i++) Serial.printf("[diag] %s\n", lines[i].text);
  Serial.println();
}
}  // namespace

namespace diag {

bool requested(uint32_t windowMs, void (*tick)()) {
  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  Serial.printf("Press BOOT within %lu ms for hardware bring-up mode\n", (unsigned long)windowMs);
  uint32_t start = millis();
  while (millis() - start < windowMs) {
    if (digitalRead(PIN_BOOT_BTN) == LOW) return true;
    if (tick) tick();
    else delay(10);
  }
  return false;
}

void run(Scanner* const* list, size_t count) {
  Serial.println("[diag] hardware bring-up mode; press RESET to exit");
  display::setBrightness(100);
  fx::begin();
  fx::setSound(true);
  gps::begin();
  checkSd();
  checkI2c();

  scanners = list;
  scannerCount = count < kMaxRadios ? count : kMaxRadios;
  for (size_t i = 0; i < scannerCount; i++) radios[i].ready = scanners[i]->begin();

  uint32_t lastDraw = 0, lastPrint = 0;
  for (;;) {
    uint32_t now = millis();
    gps::poll();
    tickScanners();
    tickTouch();
    tickAht(now);
    battery::poll();
    tickLed(now);
    fx::update();
    if (now - lastDraw >= UI_FRAME_MS) {
      lastDraw = now;
      build();
      draw();
    }
    if (now - lastPrint >= 2000) {
      lastPrint = now;
      build();
      printSerial();
    }
    delay(2);
  }
}

}  // namespace diag
