#include "Ui.h"
#include "../core/Achievements.h"
#include "../core/Engine.h"
#include "../hal/Display.h"
#include "../hal/Fx.h"
#include "../hal/Gps.h"
#include "../hal/Storage.h"
#include "../hal/Touch.h"
#include "../scanners/ScanManager.h"
#include "board.h"
#include "config.h"

namespace {
// Palette (RGB565)
const uint16_t BG = 0x0861;
const uint16_t PANEL = 0x18E4;
const uint16_t TEXT = 0xFFFF;
const uint16_t DIM = 0x8C71;
const uint16_t ACCENT = 0xFD20;
const uint16_t GOOD = 0x5E8A;
const uint16_t XPBAR = 0x2D7F;

const int16_t STATUS_H = 18;
const int16_t TAB_H = 34;
const int16_t BODY_Y = STATUS_H;
const int16_t BODY_H = SCREEN_H - STATUS_H - TAB_H;

enum class Screen : uint8_t { Home, Stats, Badges, Settings, TouchTest };
const char* kTabs[] = {"HOME", "STATS", "BADGES", "SETUP"};

Companion* pet = nullptr;
ScanManager* scanMgr = nullptr;
Screen screen = Screen::Home;
uint32_t tick = 0;
int badgePage = 0;

char toastText[40] = {0};
uint16_t toastColor = ACCENT;
uint32_t toastUntil = 0;

int16_t tapX = -1, tapY = -1;  // touch test marker

Arduino_Canvas* g() { return display::gfx(); }

void text(int16_t x, int16_t y, const char* s, uint16_t c = TEXT, uint8_t size = 1) {
  g()->setTextSize(size);
  g()->setTextColor(c);
  g()->setCursor(x, y);
  g()->print(s);
}

void textf(int16_t x, int16_t y, uint16_t c, uint8_t size, const char* fmt, ...) {
  char buf[64];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  text(x, y, buf, c, size);
}

// ---------------------------------------------------------------------------
void drawStatusBar() {
  g()->fillRect(0, 0, SCREEN_W, STATUS_H, PANEL);
  text(4, 5, "NG SCOUT", ACCENT);
  const char* active = scanMgr ? scanMgr->activeName() : nullptr;
  if (active) textf(70, 5, GOOD, 1, "%s scan", active);

  int16_t x = SCREEN_W - 4;
  // Battery: no sense circuit on this board yet -> show power source.
  x -= 24; text(x, 5, "USB", DIM);
  x -= 30; text(x, 5, "SD", storage::ok() ? GOOD : 0xF800);
  x -= 50;
  if (!gps::present()) text(x, 5, "GPS --", DIM);
  else if (gps::hasFix()) textf(x, 5, GOOD, 1, "GPS %u", gps::satellites());
  else text(x, 5, "GPS ..", ACCENT);
}

void drawTabs() {
  int16_t y = SCREEN_H - TAB_H;
  int16_t w = SCREEN_W / 4;
  for (int i = 0; i < 4; i++) {
    bool sel = (int)screen == i || (screen == Screen::TouchTest && i == 3);
    g()->fillRect(i * w, y, w, TAB_H, sel ? ACCENT : PANEL);
    g()->drawFastVLine(i * w, y, TAB_H, BG);
    int16_t tw = strlen(kTabs[i]) * 6;
    text(i * w + (w - tw) / 2, y + 13, kTabs[i], sel ? BG : TEXT);
  }
}

void drawXpBar(int16_t x, int16_t y, int16_t w) {
  const Stats& s = engine.stats();
  uint16_t lvl = engine.level();
  uint32_t lo = progression::xpForLevel(lvl), hi = progression::xpForLevel(lvl + 1);
  float p = hi > lo ? (float)(s.xp - lo) / (float)(hi - lo) : 0;
  g()->drawRect(x, y, w, 10, DIM);
  g()->fillRect(x + 1, y + 1, (int16_t)((w - 2) * p), 8, XPBAR);
  textf(x, y + 13, DIM, 1, "%lu / %lu XP", (unsigned long)s.xp, (unsigned long)hi);
}

// ---------------------------------------------------------------------------
void drawHome() {
  const Stats& s = engine.stats();
  pet->draw(g(), 8, BODY_Y + 8, tick);

  int16_t x = 150, y = BODY_Y + 8;
  uint16_t lvl = engine.level();
  textf(x, y, TEXT, 2, "LV %u", lvl);
  text(x, y + 20, progression::title(lvl), ACCENT);
  drawXpBar(x, y + 34, 160);

  y += 64;
  textf(x, y, TEXT, 1, "Wi-Fi   %lu", (unsigned long)s.wifiUnique);
  textf(x + 100, y, GOOD, 1, "+%lu", (unsigned long)s.sessWifiNew);
  y += 14;
  textf(x, y, TEXT, 1, "BLE     %lu", (unsigned long)s.bleUnique);
  textf(x + 100, y, GOOD, 1, "+%lu", (unsigned long)s.sessBleNew);
  y += 14;
  textf(x, y, TEXT, 1, "Nearby  %lu APs", (unsigned long)s.lastScanSeen);
  y += 14;
  textf(x, y, TEXT, 1, "Chans   %u", (unsigned)s.channels.count());
  y += 14;
  if (s.streak) textf(x, y, ACCENT, 1, "Streak  %lu day%s", (unsigned long)s.streak, s.streak == 1 ? "" : "s");
}

void drawStats() {
  const Stats& s = engine.stats();
  int16_t x = 10, y = BODY_Y + 8, dy = 13;
  text(x, y, "LIFETIME", ACCENT); y += dy + 2;
  textf(x, y, TEXT, 1, "Unique BSSIDs     %lu", (unsigned long)s.wifiUnique); y += dy;
  textf(x, y, TEXT, 1, "Unique SSIDs      %lu", (unsigned long)s.ssidUnique); y += dy;
  textf(x, y, TEXT, 1, "5 GHz networks    %lu", (unsigned long)s.wifi5g); y += dy;
  textf(x, y, TEXT, 1, "Open / WPA3 / Ent %lu / %lu / %lu", (unsigned long)s.wifiOpen,
        (unsigned long)s.wifiWpa3, (unsigned long)s.wifiEnterprise); y += dy;
  textf(x, y, TEXT, 1, "Best signal       %d dBm", s.bestRssi); y += dy;
  textf(x, y, TEXT, 1, "BLE devices       %lu (%lu seen)", (unsigned long)s.bleUnique,
        (unsigned long)s.bleSightings); y += dy;
  textf(x, y, TEXT, 1, "Areas explored    %lu", (unsigned long)s.geoCells); y += dy;
  textf(x, y, TEXT, 1, "Best streak       %lu", (unsigned long)s.bestStreak); y += dy;
  textf(x, y, TEXT, 1, "Scans / sessions  %lu / %lu", (unsigned long)s.scans, (unsigned long)s.sessions); y += dy;

  // channel strip: 2.4 GHz 1-14, then 5 GHz 36..177 (every 4)
  y += 4;
  text(x, y, "2.4", DIM);
  for (int c = 1; c <= 14; c++) g()->fillRect(x + 22 + (c - 1) * 8, y, 6, 7, s.channels[c] ? GOOD : PANEL);
  text(x + 140, y, "5", DIM);
  int i = 0;
  for (int c = 36; c <= 177; c += 4, i++) g()->fillRect(x + 150 + i * 4, y, 3, 7, s.channels[c] ? ACCENT : PANEL);
}

void drawBadges() {
  const Stats& s = engine.stats();
  const int perPage = 8;
  int pages = (ACHIEVEMENT_COUNT + perPage - 1) / perPage;
  if (badgePage >= pages) badgePage = 0;
  textf(10, BODY_Y + 6, ACCENT, 1, "ACHIEVEMENTS %u/%u", (unsigned)s.achieved.count(), (unsigned)ACHIEVEMENT_COUNT);
  textf(SCREEN_W - 70, BODY_Y + 6, DIM, 1, "pg %d/%d >", badgePage + 1, pages);
  int16_t y = BODY_Y + 20;
  for (int i = badgePage * perPage; i < (int)ACHIEVEMENT_COUNT && i < (badgePage + 1) * perPage; i++) {
    bool got = s.achieved[i];
    g()->fillRoundRect(8, y, SCREEN_W - 16, 20, 4, got ? PANEL : BG);
    g()->fillCircle(18, y + 10, 5, got ? ACCENT : DIM);
    text(30, y + 3, ACHIEVEMENTS[i].name, got ? TEXT : DIM);
    text(30, y + 12, ACHIEVEMENTS[i].desc, DIM);
    y += 22;
  }
}

struct Row { const char* label; };
const Row kRows[] = {{"Brightness"}, {"BLE scanning"}, {"GPS"}, {"Sound"}, {"Invert colors"}, {"Sprite pack"}, {"Touch test"}};
const int kRowCount = sizeof(kRows) / sizeof(kRows[0]);
const int16_t ROW_H = 24;

void drawSettings() {
  const Settings& st = engine.settings();
  int16_t y = BODY_Y + 4;
  for (int i = 0; i < kRowCount; i++) {
    g()->fillRoundRect(6, y, SCREEN_W - 12, ROW_H - 3, 4, PANEL);
    text(14, y + 7, kRows[i].label);
    char v[24];
    switch (i) {
      case 0: snprintf(v, sizeof(v), "-  %u%%  +", st.brightness); break;
      case 1: strlcpy(v, st.bleScan ? "ON" : "OFF", sizeof(v)); break;
      case 2: strlcpy(v, st.gps ? "ON" : "OFF", sizeof(v)); break;
      case 3: strlcpy(v, st.sound ? "ON" : "OFF", sizeof(v)); break;
      case 4: strlcpy(v, st.invert ? "ON" : "OFF", sizeof(v)); break;
      case 5: snprintf(v, sizeof(v), "%s >", st.spritePack.c_str()); break;
      default: strlcpy(v, ">", sizeof(v));
    }
    text(SCREEN_W - 16 - strlen(v) * 6, y + 7, v, ACCENT);
    y += ROW_H;
  }
  textf(14, SCREEN_H - TAB_H - 10, DIM, 1, "fw %s", NG_FW_VERSION);
}

void drawTouchTest() {
  uint16_t rx, ry, rz;
  touch::lastRaw(rx, ry, rz);
  text(10, BODY_Y + 8, "Touch each corner; note raw values", ACCENT);
  textf(10, BODY_Y + 24, TEXT, 2, "raw x %4u", rx);
  textf(10, BODY_Y + 44, TEXT, 2, "raw y %4u", ry);
  textf(10, BODY_Y + 64, DIM, 1, "pressure %u", rz);
  textf(10, BODY_Y + 80, DIM, 1, "mapped %d,%d", tapX, tapY);
  text(10, BODY_Y + 96, "Edit TOUCH_* in include/config.h", DIM);
  if (tapX >= 0) {
    g()->drawFastHLine(tapX - 8, tapY, 17, 0xF800);
    g()->drawFastVLine(tapX, tapY - 8, 17, 0xF800);
  }
  for (int i = 0; i < 4; i++) {  // corner targets
    int16_t cx = (i & 1) ? SCREEN_W - 10 : 10;
    int16_t cy = (i & 2) ? SCREEN_H - TAB_H - 10 : BODY_Y + 10;
    g()->drawCircle(cx, cy, 5, GOOD);
  }
}

void drawToast() {
  if (!toastUntil || (int32_t)(millis() - toastUntil) >= 0) return;
  int16_t w = strlen(toastText) * 12 + 20;
  if (w > SCREEN_W - 10) w = SCREEN_W - 10;
  int16_t x = (SCREEN_W - w) / 2, y = SCREEN_H - TAB_H - 30;
  g()->fillRoundRect(x, y, w, 26, 6, toastColor);
  text(x + 10, y + 6, toastText, BG, 2);
}

void settingsTap(int16_t x, int16_t y) {
  int row = (y - BODY_Y - 4) / ROW_H;
  if (row < 0 || row >= kRowCount) return;
  Settings& st = engine.settings();
  switch (row) {
    case 0: {
      int b = st.brightness + (x < SCREEN_W - 50 ? -10 : 10);
      st.brightness = b < 10 ? 10 : (b > 100 ? 100 : b);
      display::setBrightness(st.brightness);
      break;
    }
    case 1: st.bleScan = !st.bleScan; break;
    case 2:
      st.gps = !st.gps;
      if (st.gps) gps::begin();
      break;
    case 3: st.sound = !st.sound; fx::setSound(st.sound); break;
    case 4: st.invert = !st.invert; display::setInverted(st.invert); break;
    case 5: {
      auto packs = listSpritePacks();
      if (packs.empty()) { ui::toast("No packs on SD", 0xF800); return; }
      size_t idx = 0;
      for (size_t i = 0; i < packs.size(); i++)
        if (packs[i] == st.spritePack) idx = (i + 1) % packs.size();
      st.spritePack = packs[idx];
      if (!pet->pack.load(st.spritePack)) ui::toast("Pack failed, using goblin", 0xF800);
      break;
    }
    case 6: screen = Screen::TouchTest; tapX = tapY = -1; return;
  }
  engine.settingsChanged();
}
}  // namespace

namespace ui {

void begin(Companion* companion, ScanManager* scans) {
  pet = companion;
  scanMgr = scans;
}

void splash(const char* line) {
  g()->fillScreen(BG);
  text(40, 90, "NETWORK GOBLIN", ACCENT, 2);
  text(100, 112, "S C O U T", TEXT, 2);
  text(40, 150, line, DIM);
  display::flush();
}

void toast(const char* t, uint16_t color) {
  strlcpy(toastText, t, sizeof(toastText));
  toastColor = color;
  toastUntil = millis() + 2500;
}

void draw() {
  tick++;
  g()->fillScreen(BG);
  drawStatusBar();
  switch (screen) {
    case Screen::Home: drawHome(); break;
    case Screen::Stats: drawStats(); break;
    case Screen::Badges: drawBadges(); break;
    case Screen::Settings: drawSettings(); break;
    case Screen::TouchTest: drawTouchTest(); break;
  }
  drawTabs();
  drawToast();
  display::flush();
}

void onTap(int16_t x, int16_t y) {
  fx::chirp(3200, 15);
  if (y >= SCREEN_H - TAB_H) {
    screen = (Screen)(x / (SCREEN_W / 4));
    return;
  }
  switch (screen) {
    case Screen::Badges: badgePage++; break;
    case Screen::Settings: settingsTap(x, y); break;
    case Screen::TouchTest: tapX = x; tapY = y; break;
    case Screen::Home: pet->react(CState::Excited, 1200); break;  // pet the goblin
    default: break;
  }
}

}  // namespace ui
