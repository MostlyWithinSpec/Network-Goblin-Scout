#include "Ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/Achievements.h"
#include "../core/Hats.h"
#include "../core/Quests.h"
#include "../core/Trackers.h"
#include "../social/PeerCodec.h"
#include "HatArt.h"
#include "Theme.h"
#include "Widgets.h"
#include "assets/GoblinArt.h"

extern "C" {
#include "qrcodegen.h"
}

using namespace theme;

namespace ui {
using gfx::Surface;
namespace {

const float kPi = 3.14159265f;

// ---------------------------------------------------------------------------
// State
// The first five are the tabs, in tab order.
enum class Screen : uint8_t { Home, Radar, Stats, Loot, Setup, TouchTest, Share, Disclaimer, Naming };
const int kTabCount = 5;

Hooks hooks;
uint16_t* bgCache = nullptr;
bool bgReady = false;
Companion companion;

Screen screen = Screen::Home;
int statsPage = 0;
int lootPage = 0;              // 0 quests, 1 wardrobe, 2.. trophies
int badgeDetail = -1;
uint32_t screenChangedAt = 0;
int8_t slideDir = 0;          // -1 / +1 = content slides in from the left / right
float navX = 0;               // animated tab indicator position
float setupScroll = 0, setupScrollTarget = 0;
float toggleAnim[16] = {0};
uint32_t lastNow = 0;
float dt = 0;

// Touch gesture tracking
bool touching = false;
int16_t downX = 0, downY = 0, lastX = 0, lastY = 0;
uint32_t downAt = 0;
float scrollAtDown = 0;
bool dragging = false;
int16_t tapX = -1, tapY = -1;  // touch test marker

// Displayed (tweened) counters on the home screen
float shown[5] = {0};

// Speech bubble
const char* quip = nullptr;
uint32_t quipStart = 0, quipUntil = 0, nextIdleQuip = 8000;
uint32_t quipSeed = 1;

// Banners (small notifications)
struct Banner {
  char text[40];
  Glyph icon;
  uint16_t color;
};
Banner banners[4];
int bannerCount = 0;
uint32_t bannerStart = 0;

// Overlays (full-screen moments)
enum class OvType : uint8_t { LevelUp, Achievement, Encounter, Hat, AchBatch, Tracker };
struct Overlay {
  OvType type;
  uint32_t value;
  char text[40];
  char peerName[13];
  uint16_t peerLevel;
  uint8_t peerHue;
  uint8_t peerHat;
  bool isNew;
  uint8_t ids[12];   // AchBatch: achievement indices shown
  uint8_t idCount;
  uint16_t total;    // AchBatch: how many in the batch
};
Overlay overlays[6];
int overlayCount = 0;
uint32_t overlayStart = 0;
bool overlayStarted = false;

// Particles
enum class PKind : uint8_t { Spark, Heart, Confetti, Mote };
struct Particle {
  float x, y, vx, vy, life, age;
  uint16_t c;
  PKind kind;
};
Particle parts[64];
int partCount = 0;

struct Mote {
  float x, y, speed, phase;
  uint8_t size;
};
Mote motes[26];

uint32_t rng = 0x12345678;
uint32_t rnd() {
  rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
  return rng;
}
float frand(float a, float b) { return a + (b - a) * (rnd() % 10000) / 10000.0f; }

float easeOut(float t) { t = t < 0 ? 0 : (t > 1 ? 1 : t); return 1 - (1 - t) * (1 - t) * (1 - t); }
float easeBack(float t) {
  t = t < 0 ? 0 : (t > 1 ? 1 : t);
  const float c1 = 1.70158f, c3 = c1 + 1;
  return 1 + c3 * powf(t - 1, 3) + c1 * powf(t - 1, 2);
}
float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
uint8_t a8(float v) { return (uint8_t)clampf(v, 0, 255); }

void sfx(Sfx s) { if (hooks.sfx) hooks.sfx(s); }
void led(uint8_t r, uint8_t g, uint8_t b, uint16_t ms) { if (hooks.led) hooks.led(r, g, b, ms); }

void emit(PKind k, float x, float y, int n, uint16_t c, float speed, float life) {
  for (int i = 0; i < n && partCount < 64; i++) {
    float ang = frand(0, 2 * kPi), v = frand(speed * 0.4f, speed);
    Particle& p = parts[partCount++];
    p.x = x; p.y = y;
    p.vx = cosf(ang) * v;
    p.vy = sinf(ang) * v - (k == PKind::Confetti ? speed * 0.6f : 0);
    if (k == PKind::Heart) { p.vx *= 0.4f; p.vy = -frand(25, 50); }
    p.life = life * frand(0.7f, 1.0f);
    p.age = 0;
    p.c = k == PKind::Confetti ? gfx::hue((uint8_t)rnd(), 200, 255) : c;
    p.kind = k;
  }
}

void say(const char* q, uint32_t now, uint32_t ms = 4500) {
  quip = q;
  if (now - quipStart > 1500) sfx(kSfxBabble);
  quipStart = now;
  quipUntil = now + ms;
  nextIdleQuip = now + 16000 + (rnd() % 9000);
}

const char* pick(const char* const* list, size_t n) { return list[(quipSeed = rnd()) % n]; }

const char* const kIdleQuips[] = {"Sniff sniff...", "Any shiny packets?", "I smell Wi-Fi.", "Take me for a walk!",
                                  "Need... more... networks", "Hoard's looking thin.", "So quiet here...",
                                  "Wanna go explore?"};
const char* const kFoundQuips[] = {"SHINY!", "Mine! All mine!", "Into the hoard!", "Gotcha!", "Ooh, a new one!",
                                   "Nom nom packets"};
const char* const kPetQuips[] = {"Hehe!", "That tickles!", "More pats!", "*happy goblin noises*", "Again!"};
const char* const kMeshQuips[] = {"Smart home sighted!", "Bzzz... Zigbee!", "Mesh chatter!"};
const char* const kHungryQuips[] = {"So... hungry...", "Feed me packets!", "Need new networks...", "*stomach growls*"};
const char* const kBoredQuips[] = {"Booored.", "Same old networks...", "Let's go somewhere new!", "Nothing new here..."};
const char* const kLowBattQuips[] = {"Running on fumes...", "Need... a charger...", "So... sleepy..."};
const char* const kHappyQuips[] = {"Life is good.", "Best hoard ever!", "We make a great team!", "Fully fed, fully nosy."};

// ---------------------------------------------------------------------------
// Background
void renderStaticBg(gfx::Surface& s) {
  s.gradientV(0, 0, W, H, kBgTop, kBgBottom);
  // honeycomb of faint dots
  for (int16_t y = 6, row = 0; y < H; y += 12, row++)
    for (int16_t x = (row & 1) ? 10 : 2; x < W; x += 16) s.pixel(x, y, kEdge, 90);
  // circuit traces
  uint32_t saved = rng;
  rng = 0xC0FFEE;
  for (int i = 0; i < 9; i++) {
    int16_t x = (int16_t)(rnd() % W), y = (int16_t)(rnd() % H);
    int16_t len1 = 20 + rnd() % 60, len2 = 10 + rnd() % 40;
    int dir = (rnd() & 1) ? 1 : -1;
    s.hline(dir > 0 ? x : x - len1, y, len1, kEdge, 55);
    int16_t ex = x + dir * len1;
    s.line(ex, y, ex + dir * 8, y + 8, kEdge, 55);
    s.vline(ex + dir * 8, y + 8, len2, kEdge, 55);
    s.fillCircle(x, y, 2, kEdge, 90);
    s.ring(ex + dir * 8, y + 8 + len2, 3, 1, kEdge, 90);
  }
  rng = saved;
  // vignette
  for (int16_t y = 0; y < H; y++)
    for (int16_t x = 0; x < W; x++) {
      float dx = (x - W / 2) / (float)(W / 2), dy = (y - H / 2) / (float)(H / 2);
      float d = dx * dx * 0.7f + dy * dy;
      if (d > 0.6f) s.pixel(x, y, 0x0000, a8((d - 0.6f) * 180));
    }
}

void drawBackground(gfx::Surface& s, const UiModel& m) {
  if (bgCache) {
    if (!bgReady) {
      gfx::Surface bg(bgCache, W, H);
      renderStaticBg(bg);
      bgReady = true;
    }
    memcpy(s.pixels(), bgCache, (size_t)W * H * 2);
  } else {
    renderStaticBg(s);
  }
  // drifting data motes
  for (auto& mo : motes) {
    mo.y -= mo.speed * dt;
    if (mo.y < -4) { mo.y = H + 4; mo.x = frand(0, W); }
    float tw = 0.5f + 0.5f * sinf(m.now / 600.0f + mo.phase);
    float x = mo.x + 4 * sinf(m.now / 1700.0f + mo.phase);
    uint16_t c = ((int)mo.phase & 1) ? kCyan : kGreen;
    if (mo.size > 1) s.glow(x, mo.y, 5, c, a8(60 * tw));
    s.pixel((int16_t)x, (int16_t)mo.y, c, a8(70 + 120 * tw));
  }
  // another goblin nearby: magenta aurora along the top
  if (m.nearbyCount) {
    float p = 0.5f + 0.5f * sinf(m.now / 500.0f);
    s.gradientV(0, kStatusH, W, 40, kMagenta, kBgTop, a8(40 + 40 * p));
  }
}

void drawFloor(gfx::Surface& s, uint32_t now, bool scanning) {
  // synthwave floor under the goblin
  const int16_t horizon = 150, bottom = kBodyY + kBodyH;
  const float vx = 80;
  float speed = scanning ? 1.4f : 0.5f;
  float ph = fmodf(now / 1000.0f * speed, 1.0f);
  for (int i = 0; i < 7; i++) {
    float z = (i + 1 - ph) / 7.0f;  // 0 near horizon .. 1 near viewer
    if (z <= 0) continue;
    int16_t y = horizon + (int16_t)((bottom - horizon) * z * z);
    s.hline(0, y, 156, kCyan, a8(110 * z));
  }
  for (int i = -6; i <= 6; i++) {
    int16_t xb = (int16_t)(vx + i * 26);
    s.line((int16_t)vx + i * 3, horizon, xb, bottom - 1, kCyan, 45);
  }
}

// ---------------------------------------------------------------------------
// Chrome
uint16_t radioColor(const char* name) {
  if (!name) return kDim;
  if (name[0] == 'W') return kCyan;
  if (name[0] == 'B') return kBlue;
  return kViolet;
}

void drawStatusBar(gfx::Surface& s, const UiModel& m) {
  s.fillRect(0, 0, W, kStatusH, kBgBottom, 190);
  s.hline(0, kStatusH - 1, W, kEdge, 200);
  icon(s, Glyph::Goblin, 10, 10, 13, kGreen);
  int16_t x = s.text(fTitle(), 20, 3, m.myName, kGreen);
  // active radio chip with a little equaliser
  if (m.scanning) {
    uint16_t c = radioColor(m.scanning);
    int16_t w = Surface::textWidth(fSmall(), m.scanning) + 24;
    x += 8;
    s.fillRoundRect(x, 3, w, 14, 7, c, 40);
    for (int i = 0; i < 3; i++) {
      float h = 3 + 6 * (0.5f + 0.5f * sinf(m.now / 120.0f + i * 1.9f));
      s.fillRect(x + 5 + i * 3, (int16_t)(15 - h), 2, (int16_t)h, c);
    }
    s.text(fSmall(), x + 17, 1, m.scanning, c);
  }
  // right side icons
  int16_t rx = W - 12;
  if (m.battPresent) {  // battery: outline, fill level, % and a bolt while charging
    uint16_t bc = m.battCharging ? kGreen : m.battPct <= 15 ? kRed : m.battPct <= 35 ? kAmber : kText;
    int16_t bx = W - 23;
    s.roundRect(bx, 5, 18, 10, 2, bc);
    s.fillRect(bx + 18, 8, 2, 4, bc);
    s.fillRect(bx + 2, 7, (int16_t)(14 * m.battPct / 100), 6, bc);
    if (m.battCharging) {
      s.fillTriangle(bx + 10, 3, bx + 6, 11, bx + 10, 10, kAmber);
      s.fillTriangle(bx + 8, 10, bx + 12, 9, bx + 8, 17, kAmber);
    }
    char pc[6];
    snprintf(pc, sizeof(pc), "%u%%", m.battPct);
    s.textRight(fSmall(), bx - 3, 1, pc, bc);
    rx = bx - 15 - Surface::textWidth(fSmall(), pc);
  }
  icon(s, Glyph::Sd, rx, 10, 12, m.sd ? kGreen : kRed);
  rx -= 20;
  uint16_t gc = !m.gpsPresent ? kFaint : m.gpsFix ? kGreen : kAmber;
  icon(s, Glyph::Gps, rx, 10, 12, gc);
  if (m.gpsFix) {
    char sat[4];
    snprintf(sat, sizeof(sat), "%u", m.sats);
    s.text(fSmall(), rx + 7, 4, sat, kGreen);
    rx -= 8;
  }
  rx -= 20;
  icon(s, Glyph::Beacon, rx, 10, 13, m.beaconOn ? kMagenta : kFaint, m.beaconOn ? 255 : 160);
  if (m.nearbyCount) {
    rx -= 26;
    float p = 0.5f + 0.5f * sinf(m.now / 250.0f);
    s.glow(rx, 10, 12, kMagenta, a8(80 * p));
    icon(s, Glyph::Goblin, rx, 10, 13, kMagenta);
    char n[4];
    snprintf(n, sizeof(n), "%u", m.nearbyCount);
    s.text(fSmall(), rx + 8, 2, n, kMagenta);
  }
}

const char* const kTabs[] = {"HOME", "RADAR", "STATS", "LOOT", "SETUP"};
const Glyph kTabIcons[] = {Glyph::Home, Glyph::Beacon, Glyph::Stats, Glyph::Trophy, Glyph::Gear};
const int16_t kTabW = W / kTabCount;

void drawNav(gfx::Surface& s) {
  int16_t y = H - kNavH;
  s.fillRect(0, y, W, kNavH, kBgBottom, 220);
  s.hline(0, y, W, kEdge);
  int active = screen == Screen::TouchTest || screen == Screen::Share ? 4 : (int)screen;
  float target = active * (float)kTabW;
  navX += (target - navX) * clampf(dt * 14, 0, 1);
  s.fillRoundRect((int16_t)navX + 3, y + 4, kTabW - 6, kNavH - 8, 8, kCyan, 35);
  s.fillRoundRect((int16_t)navX + kTabW / 2 - 16, y + 1, 32, 3, 1, kCyan);
  s.glow(navX + kTabW / 2, y + 2, 20, kCyan, 70);
  for (int i = 0; i < kTabCount; i++) {
    uint16_t c = i == active ? kCyan : kDim;
    int16_t cx = i * kTabW + kTabW / 2;
    icon(s, kTabIcons[i], cx, y + 12, 13, c);
    s.textCentered(fSmall(), cx, y + 19, kTabs[i], c);
  }
}

// ---------------------------------------------------------------------------
// Home
void statTile(gfx::Surface& s, int16_t x, int16_t y, Glyph g, uint16_t c, const char* label, float value,
              uint32_t fresh) {
  panel(s, x, y, 74, 42);
  icon(s, g, x + 11, y + 11, 12, c);
  s.text(fSmall(), x + 21, y + 3, label, kDim);
  char buf[16];
  formatCount(buf, sizeof(buf), (uint32_t)(value + 0.5f));
  s.text(fBody(), x + 7, y + 19, buf, kText);
  if (fresh) {
    char f[12];
    snprintf(f, sizeof(f), "+%lu", (unsigned long)fresh);
    s.textRight(fSmall(), x + 70, y + 22, f, kGreen);
  }
}

void drawBubble(gfx::Surface& s, const char* text, int16_t tailX, int16_t y, uint32_t now) {
  float t = easeOut((int32_t)(now - quipStart) / 200.0f);  // pop in
  int16_t w = Surface::textWidth(fSmall(), text) + 16;
  int16_t x = clampf(tailX - w / 2, 4, 154 - w);
  uint8_t a = a8(230 * t);
  s.fillRoundRect(x, y, w, 20, 9, kText, a);
  s.fillTriangle(tailX - 5, y + 19, tailX + 5, y + 19, tailX - 2, y + 26, kText, a);
  s.text(fSmall(), x + 8, y + 2, text, kBgBottom, a);
}

void meter(gfx::Surface& s, int16_t x, int16_t y, Glyph g, const char* label, uint16_t level, uint16_t c) {
  // level 0..1000 of *need*; the bar shows how satisfied the goblin is
  float full = 1 - level / 1000.0f;
  icon(s, g, x + 5, y + 5, 10, c);
  s.text(fSmall(), x + 13, y - 3, label, kDim);
  bar(s, x + 42, y + 2, 46, 5, full, full < 0.3f ? kRed : c);
}

void drawHome(gfx::Surface& s, const UiModel& m) {
  const Stats& st = *m.stats;
  bool scanning = companion.state(m.now) == CState::Scanning;
  drawFloor(s, m.now, scanning);

  // scanning radar pulses behind the goblin
  if (m.scanning) {
    for (int i = 0; i < 3; i++) {
      float ph = fmodf(m.now / 1600.0f + i / 3.0f, 1.0f);
      s.ring(80, 128, 20 + ph * 70, 1.5f, radioColor(m.scanning), a8(90 * (1 - ph)));
    }
  }
  const Frame* custom = m.packFrame ? m.packFrame(companion.state(m.now), m.now / 300) : nullptr;
  companion.draw(s, 80, 188, m.now, custom);

  meter(s, 6, kBodyY + 6, Glyph::Heart, "FOOD", m.hunger, kGreen);
  meter(s, 6, kBodyY + 18, Glyph::Star, "FUN", m.boredom, kAmber);
  if (quip && (int32_t)(quipUntil - m.now) > 0) drawBubble(s, quip, 84, kBodyY + 34, m.now);

  // level ring + XP
  float p = m.xpHi > m.xpLo ? (float)(st.xp - m.xpLo) / (float)(m.xpHi - m.xpLo) : 0;
  progressRing(s, 190, 60, 31, 6, p, m.now);
  s.textCentered(fSmall(), 190, 35, "LEVEL", kDim);
  char lv[8];
  snprintf(lv, sizeof(lv), "%u", m.level);
  s.textCentered(fBig(), 190, 48, lv, kText);

  char buf[24];
  s.text(fSmall(), 230, 30, "XP", kDim);
  formatCount(buf, sizeof(buf), st.xp);
  s.text(fBody(), 230, 43, buf, kText);
  formatCount(buf, sizeof(buf), m.xpHi);
  char of[40];
  snprintf(of, sizeof(of), "next %s", buf);
  s.text(fSmall(), 230, 61, of, kDim);
  if (st.sessXp) {
    snprintf(buf, sizeof(buf), "+%lu today", (unsigned long)st.sessXp);
    s.text(fSmall(), 230, 75, buf, kGreen);
  }
  s.textCentered(fSmall(), 238, 95, progression::title(m.level), kAmber);

  // counters tween toward their real values
  float target[4] = {(float)st.wifiUnique, (float)st.bleUnique, (float)st.t154Unique, (float)st.peersMet};
  for (int i = 0; i < 4; i++) {
    float d = target[i] - shown[i];
    shown[i] += fabsf(d) < 1 ? d : d * clampf(dt * 5, 0, 1);
  }
  statTile(s, 160, 114, Glyph::Wifi, kCyan, "WI-FI", shown[0], st.sessWifiNew);
  statTile(s, 240, 114, Glyph::Ble, kBlue, "BLE", shown[1], st.sessBleNew);
  statTile(s, 160, 160, Glyph::Mesh, kViolet, "MESH", shown[2], st.sess154New);
  statTile(s, 240, 160, Glyph::Goblin, kMagenta, "GOBLINS", shown[3], 0);
}

// ---------------------------------------------------------------------------
// Stats
void header(gfx::Surface& s, const char* title, const char* right) {
  s.text(fTitle(), 12, kBodyY + 6, title, kCyan);
  if (right) s.textRight(fSmall(), W - 12, kBodyY + 4, right, kDim);
}

void pageDots(gfx::Surface& s, int page, int pages, int16_t y) {
  int16_t x0 = W / 2 - (pages - 1) * 5;
  for (int i = 0; i < pages; i++) s.fillCircle(x0 + i * 10, y, i == page ? 3 : 2, i == page ? kCyan : kFaint);
}

void channelRow(gfx::Surface& s, int16_t y, const char* label, const uint8_t* chans, int n, bool (*seen)(const Stats&, int),
                const Stats& st, uint16_t c, int16_t barW, int16_t gap, uint32_t now) {
  s.text(fSmall(), 18, y + 4, label, kDim);
  int16_t x = 70;
  for (int i = 0; i < n; i++) {
    bool on = seen(st, chans[i]);
    if (on) {
      float shimmer = 0.75f + 0.25f * sinf(now / 300.0f + i * 0.7f);
      s.glow(x + barW / 2.0f, y + 12, barW + 4, c, a8(50 * shimmer));
      s.gradientV(x, y + 2, barW, 20, gfx::mix(c, 0xFFFF, 90), c, a8(255 * shimmer));
    } else {
      s.fillRect(x, y + 14, barW, 8, kFaint, 140);
    }
    x += barW + gap;
  }
}

bool seenWifi(const Stats& st, int ch) { return st.channels[ch]; }
bool seen154(const Stats& st, int ch) { return st.channels154[ch]; }

void drawStats(gfx::Surface& s, const UiModel& m) {
  const Stats& st = *m.stats;
  if (statsPage == 0) {
    header(s, "SPECTRUM", "swipe for records >");
    panel(s, 8, kBodyY + 24, W - 16, 94);
    static const uint8_t c24[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    static const uint8_t c5[] = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116,
                                 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165};
    static const uint8_t c154[] = {11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26};
    channelRow(s, kBodyY + 30, "2.4 GHz", c24, 14, seenWifi, st, kCyan, 13, 4, m.now);
    channelRow(s, kBodyY + 58, "5 GHz", c5, 25, seenWifi, st, kGreen, 6, 3, m.now);
    channelRow(s, kBodyY + 86, "MESH", c154, 16, seen154, st, kViolet, 11, 4, m.now);

    // security mix
    panel(s, 8, kBodyY + 124, W - 16, 56);
    s.text(fSmall(), 18, kBodyY + 127, "SECURITY", kDim);
    uint32_t known = st.wifiOpen + st.wifiWep + st.wifiWpa3 + st.wifiEnterprise;
    uint32_t wpa2 = st.wifiUnique > known ? st.wifiUnique - known : 0;
    struct { uint32_t v; uint16_t c; const char* n; } seg[] = {
        {st.wifiOpen, kRed, "OPEN"}, {st.wifiWep, kMagenta, "WEP"}, {wpa2, kCyan, "WPA2"},
        {st.wifiWpa3, kGreen, "WPA3"}, {st.wifiEnterprise, kAmber, "ENTERPRISE"}};
    uint32_t total = st.wifiUnique ? st.wifiUnique : 1;
    int16_t x = 84, bw = W - 84 - 18;
    s.fillRoundRect(x, kBodyY + 133, bw, 7, 3, kFaint, 160);
    for (auto& sg : seg) {
      int16_t w = (int16_t)((uint64_t)bw * sg.v / total);
      if (w > 0) s.fillRect(x, kBodyY + 133, w, 7, sg.c);
      x += w;
    }
    for (int i = 0; i < 5; i++) {
      int16_t lx = 18 + (i % 3) * 98, ly = kBodyY + 145 + (i / 3) * 15;
      s.fillCircle(lx + 3, ly + 7, 3, seg[i].c);
      char b[24];
      snprintf(b, sizeof(b), "%s %lu", seg[i].n, (unsigned long)seg[i].v);
      s.text(fSmall(), lx + 10, ly, b, kDim);
    }
  } else {
    header(s, "RECORDS", "< spectrum");
    panel(s, 8, kBodyY + 24, W - 16, 152);
    char v[16][24];
    const char* k[16] = {"Best signal", "Weakest", "Busiest scan", "BLE crowd", "Hidden nets", "Wi-Fi 6",
                         "WPS routers", "Beacons", "Zigbee nets", "Thread nets", "Goblins met", "Encounters",
                         "Areas (GPS)", "Best streak", "Scanning", "Scans"};
    snprintf(v[0], 24, "%d dBm", st.bestRssi);
    snprintf(v[1], 24, "%d dBm", st.worstRssi);
    snprintf(v[2], 24, "%lu APs", (unsigned long)st.maxApsInScan);
    snprintf(v[3], 24, "%lu", (unsigned long)st.maxBleInScan);
    formatCount(v[4], 24, st.wifiHidden);
    formatCount(v[5], 24, st.wifi6);
    formatCount(v[6], 24, st.wifiWps);
    snprintf(v[7], 24, "%lu", (unsigned long)(st.bleIBeacon + st.bleEddystone));
    formatCount(v[8], 24, st.zigbeePans);
    formatCount(v[9], 24, st.threadPans);
    formatCount(v[10], 24, st.peersMet);
    formatCount(v[11], 24, st.peerEncounters);
    formatCount(v[12], 24, st.geoCells);
    snprintf(v[13], 24, "%lu d", (unsigned long)st.bestStreak);
    snprintf(v[14], 24, "%lu h", (unsigned long)(st.uptimeMin / 60));
    formatCount(v[15], 24, st.scans);
    for (int i = 0; i < 16; i++) {
      int16_t col = i % 2, row = i / 2;
      int16_t x = 18 + col * 150, y = kBodyY + 30 + row * 18;
      s.text(fSmall(), x, y, k[i], kDim);
      s.textRight(fSmall(), x + 138, y, v[i], kText);
    }
  }
  pageDots(s, statsPage, 2, kBodyY + kBodyH - 6);
}

// ---------------------------------------------------------------------------
// Badges
const int kPerPage = 18;
int trophyPages() { return (int)((ACHIEVEMENT_COUNT + kPerPage - 1) / kPerPage); }
int lootPages() { return 2 + trophyPages(); }  // quests, wardrobe, trophies...

void cellCenter(int i, int16_t& cx, int16_t& cy) {
  int col = i % 6, row = i / 6;
  cx = 32 + col * 51;
  cy = kBodyY + 52 + row * 50;
}

// Greedy word wrap into up to maxLines lines of at most 63 chars.
int wrap(const gfx::Font& f, const char* text, int16_t maxW, char out[][64], int maxLines) {
  int lines = 0;
  size_t len = 0;
  char cur[64] = "";
  const char* p = text;
  while (*p && lines < maxLines) {
    size_t wl = strcspn(p, " ");
    if (wl > 40) wl = 40;
    char trial[64];
    memcpy(trial, cur, len);
    size_t tl = len;
    if (tl) trial[tl++] = ' ';
    bool fits = tl + wl < sizeof(trial);  // also break when the buffer (not the pixels) is full
    if (fits) {
      memcpy(trial + tl, p, wl);
      tl += wl;
      trial[tl] = 0;
    }
    if (len && (!fits || Surface::textWidth(f, trial) > maxW)) {
      memcpy(out[lines++], cur, len + 1);
      memcpy(cur, p, wl);
      cur[wl] = 0;
      len = wl;
    } else {
      memcpy(cur, trial, tl + 1);
      len = tl;
    }
    p += wl;
    while (*p == ' ') p++;
  }
  if (len && lines < maxLines) memcpy(out[lines++], cur, len + 1);
  return lines;
}

void drawBadgeDetail(gfx::Surface& s, const UiModel& m) {
  const AchievementDef& a = ACHIEVEMENTS[badgeDetail];
  bool got = m.stats->achieved[badgeDetail];
  s.fillRect(0, 0, W, H, 0x0000, 150);
  panel(s, 24, 46, W - 48, 140, got ? tierColor(a.tier, m.now) : kEdge, 245);
  medallion(s, 70, 100, 30, a, got, m.now);
  s.text(fBody(), 112, 58, got || !a.secret ? a.name : "???", kText);
  s.text(fSmall(), 112, 78, tierName(a.tier), got ? tierColor(a.tier, m.now) : kDim);
  char lines[3][64];
  int n = wrap(fSmall(), got || !a.secret ? a.desc : "A secret. Keep scanning...", W - 48 - 100, lines, 3);
  for (int i = 0; i < n; i++) s.text(fSmall(), 112, 98 + i * 15, lines[i], kDim);
  if (got) {
    icon(s, Glyph::Check, 118, 166, 12, kGreen);
    s.text(fSmall(), 128, 158, "UNLOCKED", kGreen);
  } else {
    icon(s, Glyph::Lock, 118, 166, 11, kDim);
    s.text(fSmall(), 128, 158, "LOCKED", kDim);
  }
}

void drawBadges(gfx::Surface& s, const UiModel& m) {
  const Stats& st = *m.stats;
  char hdr[24];
  snprintf(hdr, sizeof(hdr), "%u / %u", (unsigned)st.achieved.count(), (unsigned)ACHIEVEMENT_COUNT);
  s.text(fTitle(), 12, kBodyY + 6, "TROPHIES", kCyan);
  s.textRight(fBody(), W - 12, kBodyY + 1, hdr, kText);
  bar(s, 112, kBodyY + 10, 110, 6, (float)st.achieved.count() / ACHIEVEMENT_COUNT, kGoldC);
  int first = (lootPage - 2) * kPerPage;
  for (int i = 0; i < kPerPage && first + i < (int)ACHIEVEMENT_COUNT; i++) {
    int16_t cx, cy;
    cellCenter(i, cx, cy);
    medallion(s, cx, cy, 18, ACHIEVEMENTS[first + i], st.achieved[first + i], m.now);
  }
}


// ---------------------------------------------------------------------------
// Loot: quests and wardrobe (trophies are drawBadges above)
void drawQuests(gfx::Surface& s, const UiModel& m) {
  s.text(fTitle(), 12, kBodyY + 6, "QUESTS", kCyan);
  char b[48];
  snprintf(b, sizeof(b), "%lu done", (unsigned long)m.stats->questsDone);
  s.textRight(fSmall(), W - 12, kBodyY + 4, b, kDim);
  const QuestBoard* qb = m.quests;
  if (!qb || !qb->active) {
    panel(s, 8, kBodyY + 30, W - 16, 120);
    icon(s, Glyph::Trophy, W / 2, kBodyY + 64, 30, kGoldC);
    s.textCentered(fBody(), W / 2, kBodyY + 86, "Board cleared!", kText);
    uint32_t wait = qb && qb->nextAtMin > m.uptimeMin ? qb->nextAtMin - m.uptimeMin : 0;
    snprintf(b, sizeof(b), wait ? "New quests in %lu min of scanning" : "New quests soon", (unsigned long)wait);
    s.textCentered(fSmall(), W / 2, kBodyY + 110, b, kDim);
    return;
  }
  for (int i = 0; i < 3; i++) {
    const Quest& q = qb->q[i];
    int16_t y = kBodyY + 26 + i * 52;
    uint32_t prog = quests::progress(*m.stats, q);
    bool done = q.done;
    panel(s, 8, y, W - 16, 46, done ? kGreen : kEdge);
    char d[48];
    quests::describe(q, d, sizeof(d));
    s.text(fBody(), 16, y + 3, d, done ? kGreen : kText);
    bar(s, 16, y + 28, 200, 7, (float)prog / (q.target ? q.target : 1), done ? kGreen : kCyan);
    snprintf(b, sizeof(b), "%lu / %u", (unsigned long)prog, q.target);
    s.text(fSmall(), 222, y + 22, b, kDim);
    if (done) {
      icon(s, Glyph::Check, W - 26, y + 23, 16, kGreen);
    } else {
      snprintf(b, sizeof(b), "+%lu", (unsigned long)quests::reward(q));
      s.textRight(fSmall(), W - 16, y + 22, b, kAmber);
    }
  }
}

const int kHatCols = 7;
void hatCell(int i, int16_t& cx, int16_t& cy) {  // i = hat id (0 = none)
  cx = 27 + (i % kHatCols) * 44;
  cy = kBodyY + 60 + (i / kHatCols) * 56;
}

void drawWardrobe(gfx::Surface& s, const UiModel& m) {
  s.text(fTitle(), 12, kBodyY + 6, "WARDROBE", kCyan);
  s.textRight(fSmall(), W - 12, kBodyY + 4, "tap to wear", kDim);
  uint8_t worn = m.settings->hat;
  for (int i = 0; i <= hats::kCount; i++) {
    int16_t cx, cy;
    hatCell(i, cx, cy);
    bool got = i == 0 || (m.hatMask & (1u << (i - 1)));
    bool on = i == worn;
    s.fillRoundRect(cx - 20, cy - 26, 40, 50, 7, on ? kPanelHi : kPanel, 230);
    s.roundRect(cx - 20, cy - 26, 40, 50, 7, on ? kCyan : kEdge, on ? 255 : 150);
    if (on) s.glow(cx, cy, 26, kCyan, 50);
    if (i == 0) {
      s.textCentered(fSmall(), cx, cy - 8, "none", kDim);
    } else if (got) {
      drawHat(s, (uint8_t)i, cx, cy + 12, 0.85f, m.now, false);
    } else {
      icon(s, Glyph::Lock, cx, cy - 2, 14, kFaint);
    }
  }
  // what the selected / next locked hat needs
  int show = worn;
  for (int i = 1; i <= hats::kCount && !show; i++)
    if (!(m.hatMask & (1u << (i - 1)))) show = -i;
  if (show > 0) {
    char b[48];
    snprintf(b, sizeof(b), "Wearing: %s", hats::kHats[show - 1].name);
    s.textCentered(fSmall(), W / 2, kBodyY + kBodyH - 30, b, kGreen);
  } else if (show < 0) {
    char b[64];
    snprintf(b, sizeof(b), "Next: %s - %s", hats::kHats[-show - 1].name, hats::kHats[-show - 1].how);
    s.textCentered(fSmall(), W / 2, kBodyY + kBodyH - 30, b, kDim);
  }
}

void drawLoot(gfx::Surface& s, const UiModel& m) {
  if (lootPage == 0) drawQuests(s, m);
  else if (lootPage == 1) drawWardrobe(s, m);
  else drawBadges(s, m);
  pageDots(s, lootPage, lootPages(), kBodyY + kBodyH - 6);
}

// ---------------------------------------------------------------------------
// Radar: everything heard in the last minute. Distance from the centre = signal
// strength (strong = close), angle = from the salted id (stable, but meaningless).
int16_t sinTab[1024];  // sin over a quarter... full turn in 1024 steps, scaled by 16384
void initSinTab() {
  for (int i = 0; i < 1024; i++) sinTab[i] = (int16_t)lroundf(sinf(i * 6.2831853f / 1024) * 16384);
}
inline int32_t isin(uint32_t a) { return sinTab[a & 1023]; }        // a: 1024 per turn
inline int32_t icos(uint32_t a) { return sinTab[(a + 256) & 1023]; }

uint16_t blipColor(Radio r) {
  switch (r) {
    case Radio::WiFi: return kCyan;
    case Radio::BLE: return kBlue;
    case Radio::Thread: return kViolet;
    default: return kMagenta;
  }
}

void drawRadar(gfx::Surface& s, const UiModel& m) {
  const int16_t cx = 106, cy = kBodyY + 92, R = 84;
  // grid
  s.fillCircle(cx, cy, R, gfx::hex(0x04161A), 200);
  for (int i = 1; i <= 3; i++) s.ring(cx, cy, R * i / 3.0f, 1, kEdge, 160);
  s.hline(cx - R, cy, 2 * R, kEdge, 90);
  s.vline(cx, cy - R, 2 * R, kEdge, 90);
  // sweep: bright leading edge and a fading trail
  uint32_t sweep = (m.now * 1024 / 2600) & 1023;  // one turn per 2.6 s
  for (int k = 0; k < 6; k++) {
    uint32_t a0 = (sweep - k * 14) & 1023, a1 = (sweep - (k + 1) * 14) & 1023;
    s.fillTriangle(cx, cy, cx + (int16_t)(isin(a0) * R / 16384), cy - (int16_t)(icos(a0) * R / 16384),
                   cx + (int16_t)(isin(a1) * R / 16384), cy - (int16_t)(icos(a1) * R / 16384), kGreen,
                   (uint8_t)(60 - k * 9));
  }
  s.line(cx, cy, cx + (int16_t)(isin(sweep) * R / 16384), cy - (int16_t)(icos(sweep) * R / 16384), kGreen);

  // blips
  uint16_t counts[5] = {0};
  int8_t best = -127;
  for (size_t i = 0; i < m.blipCount; i++) {
    const Blip& b = m.blips[i];
    if (!b.lastSeenMs) continue;
    uint32_t age = m.now - b.lastSeenMs;
    if (age > 60000) continue;
    counts[(int)b.radio]++;
    if (b.rssi > best) best = b.rssi;
    int32_t rssi = b.rssi < -100 ? -100 : (b.rssi > -30 ? -30 : b.rssi);
    int32_t dist = 10 + (-30 - rssi) * (R - 14) / 70;  // -30 dBm near the middle, -100 at the edge
    uint32_t a = b.angle >> 2;                         // 4096 -> 1024 steps
    int16_t bx = cx + (int16_t)(isin(a) * dist / 16384), by = cy - (int16_t)(icos(a) * dist / 16384);
    uint32_t since = (sweep - a) & 1023;               // how long ago the sweep passed it
    int32_t lit = 255 - (int32_t)since / 3;
    int32_t fade = 255 - (int32_t)(age / 240);         // older sightings fade out over a minute
    int32_t al = (lit < 90 ? 90 : lit) * (fade < 0 ? 0 : fade) / 255;
    uint16_t c = blipColor(b.radio);
    if (b.radio == Radio::Peer) {
      s.glow(bx, by, 10, kMagenta, (uint8_t)al);
      icon(s, Glyph::Goblin, bx, by, 11, kMagenta, (uint8_t)(al > 120 ? 255 : al * 2));
    } else {
      if (lit > 200) s.glow(bx, by, 5, c, (uint8_t)(al / 2));
      s.fillCircle(bx, by, 1.6f, c, (uint8_t)al);
    }
  }

  // legend
  int16_t x = 206, y = kBodyY + 8;
  s.text(fTitle(), x, y, "LIVE", kGreen);
  s.text(fSmall(), x + 44, y + 1, "last 60 s", kDim);
  struct { Radio r; const char* n; Glyph g; } rows[] = {{Radio::WiFi, "Wi-Fi", Glyph::Wifi},
                                                        {Radio::BLE, "Bluetooth", Glyph::Ble},
                                                        {Radio::Thread, "Mesh", Glyph::Mesh},
                                                        {Radio::Peer, "Goblins", Glyph::Goblin}};
  for (int i = 0; i < 4; i++) {
    int16_t ry = y + 24 + i * 26;
    uint16_t c = blipColor(rows[i].r);
    icon(s, rows[i].g, x + 7, ry + 9, 12, c);
    s.text(fSmall(), x + 18, ry + 1, rows[i].n, kDim);
    char n[8];
    snprintf(n, sizeof(n), "%u", counts[(int)rows[i].r]);
    s.textRight(fBody(), W - 10, ry - 1, n, kText);
  }
  char b[24];
  if (m.trackersNearby) snprintf(b, sizeof(b), "trackers near: %u", m.trackersNearby);
  else if (best > -127) snprintf(b, sizeof(b), "loudest %d dBm", best);
  else snprintf(b, sizeof(b), "listening...");
  s.text(fSmall(), x, y + 132, b, m.trackersNearby ? kAmber : kDim);
  if (m.scanning) {
    snprintf(b, sizeof(b), "now: %s", m.scanning);
    s.text(fSmall(), x, y + 148, b, radioColor(m.scanning));
  }
}

// ---------------------------------------------------------------------------
// Share card: a photo-friendly trading card with a QR code to the project.
uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(8)];
bool qrOk = false;
char qrFor[96] = "";

void drawShare(gfx::Surface& s, const UiModel& m) {
  if (strcmp(qrFor, m.shareUrl) != 0) {  // encode once per URL
    uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(8)];
    snprintf(qrFor, sizeof(qrFor), "%s", m.shareUrl);
    qrOk = qrcodegen_encodeText(qrFor, tmp, qr, qrcodegen_Ecc_MEDIUM, 1, 8, qrcodegen_Mask_AUTO, true);
  }
  const Stats& st = *m.stats;
  // card
  s.fillRoundRect(6, kBodyY + 4, W - 12, kBodyH - 8, 10, gfx::hex(0x0A1C22), 245);
  s.roundRect(6, kBodyY + 4, W - 12, kBodyH - 8, 10, kGoldC);
  s.roundRect(8, kBodyY + 6, W - 16, kBodyH - 12, 9, kGoldC, 90);
  // goblin with its hat
  s.glow(62, kBodyY + 92, 56, kCyan, 70);
  Companion::Look look;
  look.scale = 1.5f;
  look.hat = m.settings->hat;
  Companion::drawSmall(s, 62, kBodyY + 150, m.now, look);
  // name + level
  s.text(fBig(), 120, kBodyY + 10, m.myName, kGreen);
  char b[48];
  snprintf(b, sizeof(b), "LV %u  %s", m.level, progression::title(m.level));
  s.text(fSmall(), 121, kBodyY + 36, b, kAmber);
  // stats
  const struct { const char* k; uint32_t v; uint16_t c; } rows[] = {
      {"Wi-Fi", st.wifiUnique, kCyan}, {"Bluetooth", st.bleUnique, kBlue}, {"Mesh", st.t154Unique, kViolet},
      {"Goblins", st.peersMet, kMagenta}};
  for (int i = 0; i < 4; i++) {
    int16_t y = kBodyY + 56 + i * 17;
    s.text(fSmall(), 121, y, rows[i].k, kDim);
    formatCount(b, sizeof(b), rows[i].v);
    s.textRight(fSmall(), 210, y, b, rows[i].c);
  }
  snprintf(b, sizeof(b), "%u / %u trophies", (unsigned)st.achieved.count(), (unsigned)ACHIEVEMENT_COUNT);
  s.text(fSmall(), 121, kBodyY + 128, b, kGoldC);
  // QR code (white quiet zone so phones can read it)
  if (qrOk) {
    int n = qrcodegen_getSize(qr);
    int mod = 84 / (n + 2);
    if (mod < 1) mod = 1;
    int16_t size = (int16_t)((n + 2) * mod);
    int16_t qx = W - 16 - size, qy = kBodyY + 52;
    s.fillRect(qx, qy, size, size, 0xFFFF);
    for (int yy = 0; yy < n; yy++)
      for (int xx = 0; xx < n; xx++)
        if (qrcodegen_getModule(qr, xx, yy)) s.fillRect(qx + (xx + 1) * mod, qy + (yy + 1) * mod, mod, mod, 0x0000);
    s.textCentered(fSmall(), qx + size / 2, qy + size + 3, "scan me", kDim);
  }
  s.textCentered(fSmall(), W / 2, kBodyY + kBodyH - 26, "NETWORK GOBLIN SCOUT  -  tap to close", kFaint);
}

// ---------------------------------------------------------------------------
// Setup
const int kRows = 12;
const int16_t kRowH = 38;

struct RowInfo { const char* label; const char* sub; };
const RowInfo kRowInfo[kRows] = {
    {"Brightness", nullptr},
    {"Sound", "chirps and jingles"},
    {"Bluetooth scan", "passive, never connects"},
    {"Mesh scan", "Zigbee / Thread, listen only"},
    {"Goblin beacon", "let other Scouts find me"},
    {"GPS", "streaks and exploration"},
    {"Invert colours", "if the screen looks negative"},
    {"Turbo display", "faster screen; off if it glitches"},
    {"Sprite pack", "from the SD card"},
    {"Touch test", "check calibration"},
    {"Share card", "show off your goblin"},
    {"About", nullptr},
};

bool rowToggle(const Settings& st, int i, bool& v) {
  switch (i) {
    case 1: v = st.sound; return true;
    case 2: v = st.bleScan; return true;
    case 3: v = st.scan154; return true;
    case 4: v = st.beacon; return true;
    case 5: v = st.gps; return true;
    case 6: v = st.invert; return true;
    case 7: v = st.fastDisplay; return true;
    default: return false;
  }
}

float maxScroll() { return kRows * kRowH + 8 - kBodyH; }

void drawSetup(gfx::Surface& s, const UiModel& m) {
  const Settings& st = *m.settings;
  if (!dragging) setupScroll += (setupScrollTarget - setupScroll) * clampf(dt * 12, 0, 1);
  s.setClip(0, kBodyY, W, kBodyH);
  for (int i = 0; i < kRows; i++) {
    int16_t y = (int16_t)(kBodyY + 4 + i * kRowH - setupScroll);
    if (y > kBodyY + kBodyH || y + kRowH < kBodyY) continue;
    panel(s, 8, y, W - 16, kRowH - 4);
    const RowInfo& r = kRowInfo[i];
    if (i == 11) {
      char b[64];
      snprintf(b, sizeof(b), "%s  #%08lX", m.myName, (unsigned long)m.myId);
      s.text(fBody(), 16, y + 1, b, kGreen);
      snprintf(b, sizeof(b), "tap to rename  -  NG Scout fw %s", m.fwVersion);
      s.text(fSmall(), 16, y + 17, b, kDim);
      continue;
    }
    s.text(fBody(), 16, y + (r.sub ? 1 : 8), r.label, kText);
    if (r.sub) s.text(fSmall(), 16, y + 17, r.sub, kDim);
    bool v;
    if (rowToggle(st, i, v)) {
      toggleAnim[i] += ((v ? 1.0f : 0.0f) - toggleAnim[i]) * clampf(dt * 12, 0, 1);
      toggle(s, W - 52, y + 8, toggleAnim[i]);
    } else if (i == 0) {
      s.text(fBody(), W - 150, y + 7, "-", kCyan);
      bar(s, W - 136, y + 14, 96, 6, st.brightness / 100.0f, kCyan);
      s.text(fBody(), W - 32, y + 7, "+", kCyan);
    } else if (i == 8) {
      s.textRight(fSmall(), W - 34, y + 9, m.packName, kCyan);
      icon(s, Glyph::Chevron, W - 24, y + 17, 10, kCyan);
    } else if (i == 9 || i == 10) {
      icon(s, Glyph::Chevron, W - 24, y + 17, 10, kCyan);
    }
  }
  // scrollbar
  float frac = kBodyH / (float)(kRows * kRowH + 8);
  int16_t th = (int16_t)(kBodyH * frac);
  int16_t ty = (int16_t)(kBodyY + (kBodyH - th) * clampf(setupScroll / maxScroll(), 0, 1));
  s.fillRoundRect(W - 5, ty, 3, th, 1, kCyan, 120);
  s.resetClip();
}

void drawTouchTest(gfx::Surface& s, const UiModel& m) {
  header(s, "TOUCH TEST", "tap SETUP to leave");
  panel(s, 8, kBodyY + 24, 180, 70);
  char b[48];
  snprintf(b, sizeof(b), "raw x %u   y %u", m.rawX, m.rawY);
  s.text(fBody(), 16, kBodyY + 28, b, kText);
  snprintf(b, sizeof(b), "pressure %u", m.rawZ);
  s.text(fSmall(), 16, kBodyY + 48, b, kDim);
  snprintf(b, sizeof(b), "mapped %d, %d", tapX, tapY);
  s.text(fSmall(), 16, kBodyY + 64, b, kDim);
  for (int i = 0; i < 4; i++) {
    int16_t cx = (i & 1) ? W - 12 : 12, cy = (i & 2) ? kBodyY + kBodyH - 12 : kBodyY + 12;
    s.ring(cx, cy, 6, 1.5f, kGreen);
  }
  if (tapX >= 0) {
    s.glow(tapX, tapY, 16, kRed, 120);
    s.hline(tapX - 12, tapY, 25, kRed);
    s.vline(tapX, tapY - 12, 25, kRed);
  }
}

// ---------------------------------------------------------------------------
// Banners & overlays
void drawBanner(gfx::Surface& s, uint32_t now) {
  if (!bannerCount) return;
  uint32_t hold = bannerCount > 1 ? 1500 : 2600;
  uint32_t t = now - bannerStart;
  if (t > hold + 250) {
    for (int i = 1; i < bannerCount; i++) banners[i - 1] = banners[i];
    bannerCount--;
    bannerStart = now;
    return;
  }
  float in = easeOut(t / 220.0f), out = t > hold ? easeOut((t - hold) / 250.0f) : 0;
  const Banner& b = banners[0];
  int16_t w = Surface::textWidth(fBody(), b.text) + 46;
  if (w > W - 16) w = W - 16;
  int16_t x = (W - w) / 2;
  int16_t y = (int16_t)(kStatusH + 4 - 40 * (1 - in) - 40 * out);
  s.glow(x + w / 2.0f, y + 15, w * 0.6f, b.color, 50);
  panel(s, x, y, w, 30, b.color, 235);
  s.fillCircle(x + 16, y + 15, 10, b.color, 70);
  icon(s, b.icon, x + 16, y + 15, 13, b.color);
  s.text(fBody(), x + 32, y + 5, b.text, kText);
}

void rays(gfx::Surface& s, float cx, float cy, uint16_t c, uint8_t a, float rot) {
  for (int i = 0; i < 12; i++) {
    float a0 = rot + i * kPi / 6, a1 = a0 + kPi / 18;
    s.fillTriangle((int16_t)cx, (int16_t)cy, (int16_t)(cx + cosf(a0) * 260), (int16_t)(cy + sinf(a0) * 260),
                   (int16_t)(cx + cosf(a1) * 260), (int16_t)(cy + sinf(a1) * 260), c, a);
  }
}

uint32_t overlayLength(OvType t) {
  if (t == OvType::Tracker) return 180000;  // stays until answered (or 3 minutes)
  return t == OvType::Encounter ? 5200 : t == OvType::LevelUp ? 3800 : t == OvType::Hat || t == OvType::AchBatch ? 3600 : 3300;
}

void startOverlay(uint32_t now) {
  const Overlay& o = overlays[0];
  overlayStart = now;
  overlayStarted = true;
  switch (o.type) {
    case OvType::LevelUp:
      emit(PKind::Confetti, W / 2, 100, 40, 0, 160, 2.4f);
      sfx(kSfxLevelUp);
      led(60, 40, 0, 1200);
      companion.react(CState::LevelUp, 4000, now);
      break;
    case OvType::Achievement:
      emit(PKind::Spark, W / 2, 98, 24, kGoldC, 120, 1.2f);
      sfx(kSfxAchievement);
      led(50, 50, 0, 600);
      companion.react(CState::Achievement, 3000, now);
      break;
    case OvType::AchBatch:
      emit(PKind::Spark, W / 2, 110, 30, kGoldC, 150, 1.4f);
      sfx(kSfxAchievement);
      led(50, 50, 0, 600);
      companion.react(CState::Achievement, 3000, now);
      break;
    case OvType::Hat:
      emit(PKind::Confetti, W / 2, 90, 30, 0, 140, 2.0f);
      sfx(kSfxAchievement);
      led(40, 0, 60, 800);
      break;
    case OvType::Encounter:
      sfx(kSfxEncounter);
      led(60, 0, 50, 2000);
      companion.react(CState::Excited, 5000, now);
      break;
    case OvType::Tracker:
      sfx(kSfxAlert);
      led(80, 0, 0, 3000);
      break;
  }
}

void drawLevelUp(gfx::Surface& s, const Overlay& o, uint32_t t, uint32_t now) {
  float in = easeOut(t / 300.0f);
  s.fillRect(0, 0, W, H, 0x0000, a8(232 * in));
  rays(s, W / 2, 110, kGoldC, a8(35 * in), now / 3000.0f);
  float ring = t / 700.0f;
  if (ring < 1) s.ring(W / 2, 110, 10 + ring * 150, 4, kGoldC, a8(200 * (1 - ring)));
  s.glow(W / 2, 110, 80, kGoldC, a8(90 * in));
  float pop = easeBack((t - 150) / 450.0f);
  s.textCentered(fBig(), W / 2, (int16_t)(40 - 20 * (1 - pop)), "LEVEL UP!", kGoldC, a8(255 * pop));
  char lv[12];
  snprintf(lv, sizeof(lv), "%lu", (unsigned long)o.value);
  float pulse = 1 + 0.06f * sinf(now / 120.0f);
  s.glow(W / 2, 112, 46 * pulse, kAmber, a8(140 * pop));
  s.textCentered(fHuge(), W / 2, 86, lv, kText, a8(255 * pop));
  float sub = easeOut((t - 600) / 400.0f);
  s.textCentered(fBody(), W / 2, 150, progression::title((uint16_t)o.value), kAmber, a8(255 * sub));
  s.textCentered(fSmall(), W / 2, 176, "your goblin grows stronger", kDim, a8(255 * sub));
}

void drawAchievement(gfx::Surface& s, const Overlay& o, uint32_t t, uint32_t now) {
  const AchievementDef& a = ACHIEVEMENTS[o.value];
  uint16_t tc = tierColor(a.tier, now);
  float in = easeOut(t / 300.0f);
  s.fillRect(0, 0, W, H, 0x0000, a8(232 * in));
  if (a.tier >= kGold) rays(s, W / 2, 96, tc, a8(30 * in), now / 2500.0f);
  s.textCentered(fTitle(), W / 2, 28, "ACHIEVEMENT UNLOCKED", kAmber, a8(255 * in));
  float r = 34 * easeBack(t / 500.0f);
  if (r > 1) {
    s.glow(W / 2, 96, r * 2, tc, 90);
    medallion(s, W / 2, 96, r, a, true, now);
  }
  float sub = easeOut((t - 350) / 400.0f);
  const gfx::Font& nf = Surface::textWidth(fBig(), a.name) < W - 20 ? fBig() : fBody();
  s.textCentered(nf, W / 2, 140, a.name, kText, a8(255 * sub));
  char lines[2][64];
  int n = wrap(fSmall(), a.desc, W - 40, lines, 2);
  for (int i = 0; i < n; i++) s.textCentered(fSmall(), W / 2, 168 + i * 15, lines[i], kDim, a8(255 * sub));
  char xp[24];
  snprintf(xp, sizeof(xp), "%s  +%d XP", tierName(a.tier), 25 * (1 + a.tier));
  s.textCentered(fSmall(), W / 2, 202, xp, tc, a8(255 * sub));
}

void drawEncounter(gfx::Surface& s, const Overlay& o, uint32_t t, uint32_t now, const UiModel& m) {
  float in = easeOut(t / 300.0f);
  s.fillRect(0, 0, W, H, gfx::hex(0x12031A), a8(240 * in));
  rays(s, W / 2, 120, kMagenta, a8(22 * in), -(float)now / 2800.0f);
  bool flash = (now / 180) % 2;
  s.textCentered(fBig(), W / 2, 14, "GOBLIN ENCOUNTER!", flash ? kMagenta : kText, a8(255 * in));

  float meet = easeOut(t / 900.0f);
  int16_t lx = (int16_t)(-50 + (W / 2 - 52 + 50) * meet);
  int16_t rx = (int16_t)(W + 50 - (W + 50 - (W / 2 + 52)) * meet);
  Companion::Look me;
  me.scale = 1.5f;
  me.hat = m.settings->hat;
  Companion::drawSmall(s, lx, 168, now, me);
  Companion::Look them;
  them.scale = 1.5f;
  them.flip = true;
  them.tint = true;
  them.tintColor = gfx::hue(o.peerHue, 190, 255);
  them.hat = o.peerHat;
  Companion::drawSmall(s, rx, 168, now + 333, them);

  if (t > 850) {  // sparks where they meet
    if (t < 1000 && partCount < 40) {
      emit(PKind::Spark, W / 2, 125, 18, kCyan, 140, 0.8f);
      emit(PKind::Heart, W / 2, 120, 6, kMagenta, 40, 1.6f);
    }
    uint32_t seed = now / 60;
    for (int b = 0; b < 2; b++) {  // little lightning arcs
      int16_t x0 = W / 2 - 18, y0 = 112 + b * 18;
      for (int k = 0; k < 6; k++) {
        seed = seed * 1103515245 + 12345;
        int16_t x1 = x0 + 6, y1 = y0 + (int16_t)((seed >> 16) % 13) - 6;
        s.line(x0, y0, x1, y1, b ? kCyan : 0xFFFF, 220);
        x0 = x1; y0 = y1;
      }
    }
  }
  float sub = easeOut((t - 1000) / 400.0f);
  s.textCentered(fBody(), lx, 172, "YOU", kGreen, a8(255 * sub));
  s.textCentered(fSmall(), lx, 190, m.myName, kDim, a8(255 * sub));
  s.textCentered(fBody(), rx, 172, o.peerName, gfx::hue(o.peerHue, 150, 255), a8(255 * sub));
  char lv[16];
  snprintf(lv, sizeof(lv), "LEVEL %u", o.peerLevel);
  s.textCentered(fSmall(), rx, 190, lv, kDim, a8(255 * sub));
  float last = easeBack((t - 1500) / 500.0f);
  s.textCentered(fBody(), W / 2, 212, o.isNew ? "NEW FRIEND!  +100 XP" : "REUNITED!  +25 XP",
                 o.isNew ? kGreen : kAmber, a8(255 * clampf(last, 0, 1)));
}

void drawHatOverlay(gfx::Surface& s, const Overlay& o, uint32_t t, uint32_t now) {
  float in = easeOut(t / 300.0f);
  s.fillRect(0, 0, W, H, 0x0000, a8(225 * in));
  rays(s, W / 2, 110, kMagenta, a8(25 * in), now / 3000.0f);
  s.textCentered(fBig(), W / 2, 14, "NEW HAT!", kMagenta, a8(255 * in));
  Companion::Look look;
  look.scale = 1.1f;
  look.hat = (uint8_t)o.value;
  float pop = easeBack((t - 200) / 500.0f);
  if (pop > 0.05f) {
    look.alpha = a8(255 * clampf(pop, 0, 1));
    Companion::drawGoblin(s, W / 2, 196, now, CState::Excited, look);
  }
  if (o.value >= 1 && o.value <= hats::kCount) {
    s.textCentered(fBody(), W / 2, 200, hats::kHats[o.value - 1].name, kText, a8(255 * in));
    s.textCentered(fSmall(), W / 2, 220, "wear it from LOOT > Wardrobe", kDim, a8(255 * in));
  }
}

void drawAchBatch(gfx::Surface& s, const Overlay& o, uint32_t t, uint32_t now) {
  float in = easeOut(t / 300.0f);
  s.fillRect(0, 0, W, H, 0x0000, a8(232 * in));
  rays(s, W / 2, 110, kGoldC, a8(25 * in), now / 2500.0f);
  char b[32];
  snprintf(b, sizeof(b), "+%u ACHIEVEMENTS!", o.total);
  s.textCentered(fBig(), W / 2, 22, b, kGoldC, a8(255 * in));
  int n = o.idCount;
  int cols = n < 6 ? n : 6;
  for (int i = 0; i < n; i++) {
    float pop = easeBack((t - 200 - i * 90) / 350.0f);
    if (pop <= 0.02f) continue;
    int col = i % 6, row = i / 6;
    int16_t cx = W / 2 - (cols - 1) * 22 + col * 44, cy = 90 + row * 48;
    medallion(s, cx, cy, 18 * pop, ACHIEVEMENTS[o.ids[i]], true, now);
  }
  if (o.total > o.idCount) {
    snprintf(b, sizeof(b), "...and %u more", o.total - o.idCount);
    s.textCentered(fSmall(), W / 2, 178, b, kDim, a8(255 * in));
  }
  s.textCentered(fSmall(), W / 2, 202, "see them all in LOOT", kDim, a8(255 * in));
}

// Tracker alert buttons
const int16_t kTrkBtnY = 198, kTrkBtnH = 32;
bool inMineBtn(int16_t x, int16_t y) { return y >= kTrkBtnY - 4 && x < W / 2 - 4; }
bool inOkBtn(int16_t x, int16_t y) { return y >= kTrkBtnY - 4 && x > W / 2 + 4; }

void drawTracker(gfx::Surface& s, const Overlay& o, uint32_t t, uint32_t now) {
  uint8_t kind = o.value & 0xFF, places = (o.value >> 8) & 0xFF;
  uint32_t mins = o.value >> 16;
  float in = easeOut(t / 300.0f);
  s.fillRect(0, 0, W, H, gfx::hex(0x1A0606), a8(250 * in));
  // pulsing alarm rings around a tag
  for (int i = 0; i < 3; i++) {
    float ph = fmodf(now / 1400.0f + i / 3.0f, 1.0f);
    s.ring(W / 2, 72, 14 + ph * 46, 2, kRed, a8(150 * (1 - ph) * in));
  }
  s.glow(W / 2, 72, 34, kRed, a8(90 * in));
  s.fillRoundRect(W / 2 - 13, 58, 26, 28, 9, kText, a8(255 * in));
  s.ring(W / 2, 66, 3.5f, 2, kBgBottom, a8(255 * in));
  s.fillCircle(W / 2, 77, 3, kRed, a8(255 * in));
  bool flash = (now / 400) % 2;
  s.textCentered(fTitle(), W / 2, 10, "SOMETHING IS FOLLOWING YOU", flash ? kRed : kAmber, a8(255 * in));
  s.textCentered(fBody(), W / 2, 104, trackers::kindName(kind), kText, a8(255 * in));
  char b[64];
  snprintf(b, sizeof(b), "with you for %lu min, through %u places", (unsigned long)mins, places);
  s.textCentered(fSmall(), W / 2, 128, b, kAmber, a8(255 * in));
  s.textCentered(fSmall(), W / 2, 148, "Not yours? Check your bag, pockets and car.", kDim, a8(255 * in));
  s.textCentered(fSmall(), W / 2, 164, "Your phone can find it and play its sound.", kDim, a8(255 * in));
  s.fillRoundRect(12, kTrkBtnY, W / 2 - 20, kTrkBtnH, 14, kPanel, a8(240 * in));
  s.roundRect(12, kTrkBtnY, W / 2 - 20, kTrkBtnH, 14, kDim, a8(255 * in));
  s.textCentered(fBody(), W / 4 + 2, kTrkBtnY + 5, "IT'S MINE", kText, a8(255 * in));
  s.fillRoundRect(W / 2 + 8, kTrkBtnY, W / 2 - 20, kTrkBtnH, 14, kAmber, a8(255 * in));
  s.textCentered(fBody(), W * 3 / 4 - 2, kTrkBtnY + 5, "GOT IT", kBgBottom, a8(255 * in));
}

void drawOverlay(gfx::Surface& s, const UiModel& m) {
  if (!overlayCount) return;
  if (!overlayStarted) startOverlay(m.now);
  const Overlay& o = overlays[0];
  uint32_t t = m.now - overlayStart;
  if (t > overlayLength(o.type)) {
    for (int i = 1; i < overlayCount; i++) overlays[i - 1] = overlays[i];
    overlayCount--;
    overlayStarted = false;
    return;
  }
  switch (o.type) {
    case OvType::LevelUp: drawLevelUp(s, o, t, m.now); break;
    case OvType::Achievement: drawAchievement(s, o, t, m.now); break;
    case OvType::Encounter: drawEncounter(s, o, t, m.now, m); break;
    case OvType::Hat: drawHatOverlay(s, o, t, m.now); break;
    case OvType::AchBatch: drawAchBatch(s, o, t, m.now); break;
    case OvType::Tracker: drawTracker(s, o, t, m.now); break;
  }
}

void drawParticles(gfx::Surface& s) {
  for (int i = 0; i < partCount;) {
    Particle& p = parts[i];
    p.age += dt;
    if (p.age >= p.life) { parts[i] = parts[--partCount]; continue; }
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    if (p.kind == PKind::Confetti) { p.vy += 220 * dt; p.vx *= 0.99f; }
    if (p.kind == PKind::Spark) { p.vx *= 0.94f; p.vy *= 0.94f; }
    float f = 1 - p.age / p.life;
    switch (p.kind) {
      case PKind::Spark:
        s.glow(p.x, p.y, 5, p.c, a8(160 * f));
        s.pixel((int16_t)p.x, (int16_t)p.y, 0xFFFF, a8(255 * f));
        break;
      case PKind::Heart: icon(s, Glyph::Heart, (int16_t)p.x, (int16_t)p.y, 10, p.c, a8(255 * f)); break;
      case PKind::Confetti: {
        int16_t w = (int16_t)(1 + 3 * fabsf(sinf(p.age * 9 + p.vx)));
        s.fillRect((int16_t)p.x, (int16_t)p.y, w, 4, p.c, a8(255 * f));
        break;
      }
      case PKind::Mote: break;
    }
    i++;
  }
}


// ---------------------------------------------------------------------------
// First run: disclaimer and naming
bool needName = false;
Screen afterNaming = Screen::Home;
char nameBuf[peercodec::kMaxName + 1] = "";
bool caps = true;
int lastKey = -1;
uint32_t lastKeyAt = 0;

const char* const kKeyRows[3] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
enum Special { kShift = 100, kDel, kDice, kSpace, kOk };

struct KeyRect { int16_t x, y, w, h; int code; };
int keys(KeyRect* out) {  // all keys, returns count
  int n = 0;
  for (int i = 0; i < 10; i++) out[n++] = {(int16_t)(6 + i * 31), 78, 29, 34, kKeyRows[0][i]};
  for (int i = 0; i < 9; i++) out[n++] = {(int16_t)(21 + i * 31), 116, 29, 34, kKeyRows[1][i]};
  out[n++] = {6, 154, 44, 34, kShift};
  for (int i = 0; i < 7; i++) out[n++] = {(int16_t)(54 + i * 31), 154, 29, 34, kKeyRows[2][i]};
  out[n++] = {271, 154, 43, 34, kDel};
  out[n++] = {6, 194, 74, 38, kDice};
  out[n++] = {84, 194, 140, 38, kSpace};
  out[n++] = {228, 194, 86, 38, kOk};
  return n;
}

void drawNaming(gfx::Surface& s, const UiModel& m) {
  s.textCentered(fTitle(), W / 2, 8, afterNaming == Screen::Setup ? "RENAME YOUR GOBLIN" : "NAME YOUR GOBLIN", kCyan);
  panel(s, 8, 28, W - 16, 42, kCyan);
  Companion::Look look;
  look.scale = 0.55f;
  look.aura = false;
  look.hat = m.settings->hat;
  Companion::drawSmall(s, 30, 70, m.now, look);
  if (nameBuf[0]) {
    int16_t end = s.textCentered(fBig(), W / 2 + 10, 36, nameBuf, kGreen);
    if ((m.now / 450) % 2) s.fillRect(end + 2, 38, 2, 24, kGreen);
  } else {
    s.textCentered(fBody(), W / 2 + 10, 40, "type a name, or roll the dice", kFaint);
  }
  char cnt[8];
  snprintf(cnt, sizeof(cnt), "%u/%u", (unsigned)strlen(nameBuf), (unsigned)peercodec::kMaxName);
  s.textRight(fSmall(), W - 14, 52, cnt, kDim);

  KeyRect k[32];
  int n = keys(k);
  for (int i = 0; i < n; i++) {
    bool hot = k[i].code == lastKey && m.now - lastKeyAt < 150;
    bool ok = k[i].code == kOk;
    bool enabled = !ok || nameBuf[0];
    uint16_t edge = ok ? (enabled ? kGreen : kFaint) : (k[i].code == kShift && caps ? kCyan : kEdge);
    s.fillRoundRect(k[i].x, k[i].y, k[i].w, k[i].h, 6, hot ? kCyan : (ok && enabled ? gfx::dim(kGreen, 70) : kPanel), 235);
    s.roundRect(k[i].x, k[i].y, k[i].w, k[i].h, 6, edge);
    int16_t cx = k[i].x + k[i].w / 2, ty = k[i].y + (k[i].h - 20) / 2;
    uint16_t tc = hot ? kBgBottom : kText;
    switch (k[i].code) {
      case kShift: s.textCentered(fBody(), cx, ty, caps ? "AB" : "ab", caps ? kCyan : kDim); break;
      case kDel: s.textCentered(fBody(), cx, ty, "DEL", tc); break;
      case kDice: s.textCentered(fBody(), cx, ty, "random", tc); break;
      case kSpace: s.textCentered(fBody(), cx, ty, "space", kDim); break;
      case kOk: s.textCentered(fBody(), cx, ty, "DONE", enabled ? kGreen : kFaint); break;
      default: {
        char c[2] = {(char)(caps ? k[i].code : k[i].code + 32), 0};
        s.textCentered(fBody(), cx, ty, c, tc);
      }
    }
  }
}

void finishNaming(uint32_t now) {
  size_t len = strlen(nameBuf);
  while (len && nameBuf[len - 1] == ' ') nameBuf[--len] = 0;  // trim
  if (!len) return;
  if (hooks.named) hooks.named(nameBuf);
  sfx(kSfxQuest);
  screen = afterNaming;
  screenChangedAt = now;
  companion.react(CState::Excited, 2000, now);
  static char hello[40];
  snprintf(hello, sizeof(hello), "I'm %s!", nameBuf);
  say(hello, now + 300, 3500);
}

void tapNaming(int16_t x, int16_t y, uint32_t now) {
  KeyRect k[32];
  int n = keys(k);
  for (int i = 0; i < n; i++) {
    if (x < k[i].x || x >= k[i].x + k[i].w || y < k[i].y || y >= k[i].y + k[i].h) continue;
    int code = k[i].code;
    lastKey = code;
    lastKeyAt = now;
    sfx(kSfxTap);
    size_t len = strlen(nameBuf);
    switch (code) {
      case kShift: caps = !caps; break;
      case kDel: if (len) nameBuf[len - 1] = 0; caps = len <= 1; break;
      case kDice: {
        char gen[peercodec::kMaxName + 1];
        peercodec::nameFor(rnd(), gen, sizeof(gen));
        snprintf(nameBuf, sizeof(nameBuf), "%s", gen);
        caps = false;
        break;
      }
      case kSpace: if (len && len < peercodec::kMaxName && nameBuf[len - 1] != ' ') { nameBuf[len] = ' '; nameBuf[len + 1] = 0; caps = true; } break;
      case kOk: finishNaming(now); break;
      default:
        if (len < peercodec::kMaxName) {
          nameBuf[len] = (char)(caps ? code : code + 32);
          nameBuf[len + 1] = 0;
          caps = false;  // capital first letter, then lowercase
        }
    }
    return;
  }
}

void startNaming(Screen returnTo, const char* current) {
  afterNaming = returnTo;
  snprintf(nameBuf, sizeof(nameBuf), "%s", current ? current : "");
  caps = !nameBuf[0];
  screen = Screen::Naming;
}

void drawDisclaimer(gfx::Surface& s, const UiModel& m) {
  s.textCentered(fTitle(), W / 2, 8, "BEFORE WE START", kCyan);
  panel(s, 8, 28, W - 16, 166);
  static const char* const kParas[] = {
      "NG Scout passively listens to Wi-Fi, Bluetooth and Zigbee/Thread signals around you. It never "
      "connects to, decodes or interferes with anyone's networks or devices, and only keeps scrambled IDs "
      "and counts.",
      "Rules about radio listening differ between countries. You are responsible for using it legally "
      "and respectfully.",
      "A hobby project, provided as-is with no warranty."};
  int16_t y = 33;
  for (const char* p : kParas) {
    char lines[7][64];
    int n = wrap(fSmall(), p, W - 36, lines, 7);
    for (int i = 0; i < n; i++, y += 15) s.text(fSmall(), 18, y, lines[i], kText);
    y += 6;
  }
  float pulse = 0.5f + 0.5f * sinf(m.now / 400.0f);
  s.glow(W / 2, 214, 90, kCyan, a8(40 + 40 * pulse));
  s.fillRoundRect(60, 200, W - 120, 32, 16, kCyan);
  s.textCentered(fBody(), W / 2, 205, "I UNDERSTAND", kBgBottom);
}

void tapDisclaimer(int16_t x, int16_t y, uint32_t now) {
  if (x < 60 || x > W - 60 || y < 196 || y > 236) return;
  sfx(kSfxTap);
  if (hooks.agreed) hooks.agreed();
  if (needName) {
    startNaming(Screen::Home, "");
  } else {
    screen = Screen::Home;
    screenChangedAt = now;
  }
}

// ---------------------------------------------------------------------------
// Input
void goTo(Screen sc, uint32_t now) {
  if (sc == screen) return;
  slideDir = (int)sc > (int)screen ? 1 : -1;
  screen = sc;
  screenChangedAt = now;
  badgeDetail = -1;
}

void toggleSetting(int row, const UiModel& m) {
  Settings& st = *m.settings;
  switch (row) {
    case 1: st.sound = !st.sound; if (hooks.sound) hooks.sound(st.sound); break;
    case 2: st.bleScan = !st.bleScan; break;
    case 3: st.scan154 = !st.scan154; break;
    case 4: st.beacon = !st.beacon; if (hooks.beacon) hooks.beacon(st.beacon); break;
    case 5: st.gps = !st.gps; if (hooks.gps) hooks.gps(st.gps); break;
    case 6: st.invert = !st.invert; if (hooks.invert) hooks.invert(st.invert); break;
    case 7: st.fastDisplay = !st.fastDisplay; if (hooks.fastDisplay) hooks.fastDisplay(st.fastDisplay); break;
    default: return;
  }
  if (hooks.settingsChanged) hooks.settingsChanged();
}

void onTap(int16_t x, int16_t y, const UiModel& m) {
  uint32_t now = m.now;
  if (overlayCount && screen != Screen::Disclaimer && screen != Screen::Naming) {
    if (overlays[0].type == OvType::Tracker) {  // needs an answer, not a skip
      bool mine = inMineBtn(x, y);
      if (!mine && !inOkBtn(x, y)) return;
      sfx(kSfxTap);
      if (mine && hooks.trackerMine) hooks.trackerMine();
      if (mine) say("Phew, it's ours.", now + 400, 3000);
    }
    overlayStart = now - overlayLength(overlays[0].type) + 200;  // tap skips the celebration
    return;
  }
  sfx(kSfxTap);
  if (screen == Screen::Disclaimer) { tapDisclaimer(x, y, now); return; }
  if (screen == Screen::Naming) { tapNaming(x, y, now); return; }
  if (screen == Screen::Share) {  // tap anywhere closes the card
    goTo(Screen::Home, now);
    return;
  }
  if (y >= H - kNavH) {
    int tab = x / kTabW;
    if (tab >= kTabCount) tab = kTabCount - 1;
    if ((Screen)tab == Screen::Stats && screen == Screen::Stats) statsPage ^= 1;
    else if ((Screen)tab == Screen::Loot && screen == Screen::Loot) { lootPage = (lootPage + 1) % lootPages(); badgeDetail = -1; }
    goTo((Screen)tab, now);
    return;
  }
  switch (screen) {
    case Screen::Home:
      if (x < 150 && y > 60) {  // pet the goblin
        companion.react(CState::Excited, 1400, now);
        emit(PKind::Heart, x, y, 4, kMagenta, 40, 1.4f);
        say(pick(kPetQuips, 5), now, 2500);
        sfx(kSfxPet);
        if (hooks.pet) hooks.pet();
      }
      break;
    case Screen::Stats: statsPage ^= 1; break;
    case Screen::Loot: {
      if (badgeDetail >= 0) { badgeDetail = -1; break; }
      if (lootPage == 1) {  // wardrobe: wear an unlocked hat
        for (int i = 0; i <= hats::kCount; i++) {
          int16_t cx, cy;
          hatCell(i, cx, cy);
          if (abs(x - cx) > 21 || abs(y - cy) > 26) continue;
          if (i > 0 && !(m.hatMask & (1u << (i - 1)))) { sfx(kSfxTap); return; }
          m.settings->hat = (uint8_t)i;
          companion.react(CState::Excited, 1200, now);
          if (hooks.hat) hooks.hat((uint8_t)i);
          if (hooks.settingsChanged) hooks.settingsChanged();
          return;
        }
        break;
      }
      if (lootPage < 2) break;
      for (int i = 0; i < kPerPage; i++) {
        int16_t cx, cy;
        cellCenter(i, cx, cy);
        int idx = (lootPage - 2) * kPerPage + i;
        if (idx < (int)ACHIEVEMENT_COUNT && abs(x - cx) < 24 && abs(y - cy) < 24) { badgeDetail = idx; return; }
      }
      break;
    }
    case Screen::Setup: {
      int row = (int)((y - kBodyY - 4 + setupScroll) / kRowH);
      if (row < 0 || row >= kRows) break;
      Settings& st = *m.settings;
      if (row == 0) {
        int b = st.brightness + (x < W - 90 ? -10 : 10);
        st.brightness = (uint8_t)(b < 10 ? 10 : (b > 100 ? 100 : b));
        if (hooks.brightness) hooks.brightness(st.brightness);
        if (hooks.settingsChanged) hooks.settingsChanged();
      } else if (row == 8) {
        if (hooks.nextPack && !hooks.nextPack()) {
          Banner& b = banners[bannerCount < 4 ? bannerCount++ : 3];
          strcpy(b.text, "No sprite packs on SD");
          b.icon = Glyph::Sd;
          b.color = kRed;
          if (bannerCount == 1) bannerStart = now;
        }
      } else if (row == 9) {
        tapX = tapY = -1;
        screen = Screen::TouchTest;
      } else if (row == 10) {
        screen = Screen::Share;
        screenChangedAt = now;
      } else if (row == 11) {
        startNaming(Screen::Setup, m.myName);
      } else {
        toggleSetting(row, m);
      }
      break;
    }
    case Screen::TouchTest: tapX = x; tapY = y; break;
    default: break;
  }
}

void onLongPress(int16_t x, int16_t y, uint32_t now) {
  if (overlayCount) return;
  if (screen == Screen::Home && x < 150 && y > 60) {  // hold the goblin: show its card
    screen = Screen::Share;
    screenChangedAt = now;
    sfx(kSfxQuest);
  }
}

void onSwipe(int dir, uint32_t now) {  // dir +1 = finger moved left (next)
  if (overlayCount) return;
  if (screen == Screen::Stats) { statsPage = dir > 0 ? 1 : 0; return; }
  if (screen == Screen::Loot) {
    badgeDetail = -1;
    lootPage = (lootPage + dir + lootPages()) % lootPages();
    return;
  }
  int next = (int)screen + dir;
  if ((int)screen < kTabCount && next >= 0 && next < kTabCount) goTo((Screen)next, now);
}

}  // namespace

// ---------------------------------------------------------------------------
void begin(const Hooks& h, uint16_t* bgBuffer) {
  initSinTab();
  hooks = h;
  bgCache = bgBuffer;
  bgReady = false;
  for (auto& mo : motes) {
    mo.x = frand(0, W);
    mo.y = frand(0, H);
    mo.speed = frand(4, 14);
    mo.phase = frand(0, 6.28f);
    mo.size = (uint8_t)((rnd() % 4) == 0 ? 2 : 1);
  }
}

Companion& pet() { return companion; }

void startOnboarding(bool disclaimer, bool name) {
  needName = name;
  if (disclaimer) screen = Screen::Disclaimer;
  else if (name) startNaming(Screen::Home, "");
}

bool onboarding() { return screen == Screen::Disclaimer || (screen == Screen::Naming && afterNaming == Screen::Home); }

bool animating() { return overlayCount > 0 || bannerCount > 0 || partCount > 0; }

Profile prof;
const Profile& profile() { return prof; }

void debugShow(int sc, int page) {
  screen = (Screen)sc;
  statsPage = screen == Screen::Stats ? page : 0;
  lootPage = screen == Screen::Loot ? page : 0;
  if (screen == Screen::Setup) setupScroll = setupScrollTarget = (float)page;
  badgeDetail = -1;
  screenChangedAt = 0;
}

void debugBadge(int index) { badgeDetail = index; }

void onEvent(const UiEvent& e, uint32_t now) {
  auto banner = [&](Glyph g, uint16_t c) {
    if (bannerCount == 4) { for (int i = 1; i < 4; i++) banners[i - 1] = banners[i]; bannerCount = 3; }
    Banner& b = banners[bannerCount++];
    snprintf(b.text, sizeof(b.text), "%s", e.text);
    b.icon = g;
    b.color = c;
    if (bannerCount == 1) bannerStart = now;
  };
  auto overlay = [&](OvType t) {
    if (overlayCount == 6) return;
    Overlay& o = overlays[overlayCount++];
    o.type = t;
    o.value = e.value;
    snprintf(o.text, sizeof(o.text), "%s", e.text);
    snprintf(o.peerName, sizeof(o.peerName), "%s", e.peerName);
    o.peerLevel = e.peerLevel;
    o.peerHue = e.peerHue;
    o.peerHat = e.peerHat;
    o.isNew = e.type == EventType::PeerNew;
  };
  switch (e.type) {
    case EventType::NewWifi:
      banner(Glyph::Wifi, kCyan);
      companion.react(CState::Discovered, 1500, now);
      emit(PKind::Spark, 92, 70, 10, kCyan, 70, 0.9f);
      say(pick(kFoundQuips, 6), now);
      sfx(kSfxDiscover);
      led(0, 40, 30, 150);
      break;
    case EventType::NewBle:
      banner(Glyph::Ble, kBlue);
      companion.react(CState::Discovered, 1200, now);
      sfx(kSfxDiscover);
      led(0, 10, 50, 150);
      break;
    case EventType::New154:
      banner(Glyph::Mesh, kViolet);
      companion.react(CState::Discovered, 1500, now);
      say(pick(kMeshQuips, 3), now);
      sfx(kSfxDiscover);
      led(30, 0, 50, 200);
      break;
    case EventType::NewChannel:
      banner(Glyph::Signal, kGreen);
      companion.react(CState::Excited, 1800, now);
      sfx(kSfxChannel);
      led(20, 50, 0, 200);
      break;
    case EventType::NewCell:
      banner(Glyph::Map, kAmber);
      companion.react(CState::Excited, 2000, now);
      sfx(kSfxChannel);
      break;
    case EventType::DailyBonus:
      banner(Glyph::Clock, kAmber);
      companion.react(CState::Excited, 2000, now);
      sfx(kSfxChannel);
      break;
    case EventType::LevelUp: overlay(OvType::LevelUp); break;
    case EventType::Achievement: {
      // More than two queued? Fold the rest into one "+N achievements" card.
      int pending = 0;
      for (int i = overlayStarted ? 1 : 0; i < overlayCount; i++)
        if (overlays[i].type == OvType::Achievement || overlays[i].type == OvType::AchBatch) pending++;
      if (pending < 2) { overlay(OvType::Achievement); break; }
      Overlay* batch = nullptr;
      for (int i = overlayStarted ? 1 : 0; i < overlayCount; i++)
        if (overlays[i].type == OvType::AchBatch) batch = &overlays[i];
      if (!batch) {
        if (overlayCount == 6) break;
        batch = &overlays[overlayCount++];
        batch->type = OvType::AchBatch;
        batch->idCount = 0;
        batch->total = 0;
      }
      if (batch->idCount < 12) batch->ids[batch->idCount++] = (uint8_t)e.value;
      batch->total++;
      break;
    }
    case EventType::QuestDone:
      banner(Glyph::Check, kGreen);
      companion.react(CState::Excited, 1800, now);
      emit(PKind::Spark, 92, 90, 14, kGreen, 90, 1.0f);
      say("Quest complete!", now, 2500);
      sfx(kSfxQuest);
      led(0, 60, 0, 300);
      break;
    case EventType::BoardCleared:
      banner(Glyph::Trophy, kGoldC);
      emit(PKind::Confetti, W / 2, 60, 30, 0, 140, 2.0f);
      sfx(kSfxLevelUp);
      break;
    case EventType::NewQuests:
      banner(Glyph::Trophy, kCyan);
      break;
    case EventType::HatUnlocked: overlay(OvType::Hat); break;
    case EventType::TrackerAlert:
      // Jumps the queue: safety first, celebrations can wait.
      if (overlayCount == 6) overlayCount--;
      overlay(OvType::Tracker);
      if (overlayCount > 1 && !overlayStarted) {
        Overlay tmp = overlays[overlayCount - 1];
        for (int i = overlayCount - 1; i > 0; i--) overlays[i] = overlays[i - 1];
        overlays[0] = tmp;
      } else if (overlayCount > 2) {  // one is on screen: go right after it
        Overlay tmp = overlays[overlayCount - 1];
        for (int i = overlayCount - 1; i > 1; i--) overlays[i] = overlays[i - 1];
        overlays[1] = tmp;
      }
      break;
    case EventType::PeerNew:
    case EventType::PeerReunion:
      overlay(OvType::Encounter);
      say(e.type == EventType::PeerNew ? "A new friend!" : "Hey, it's you again!", now + 5200, 4000);
      break;
  }
}

void notice(const char* text, uint32_t now) {
  if (bannerCount == 4) return;
  Banner& b = banners[bannerCount++];
  snprintf(b.text, sizeof(b.text), "%s", text);
  b.icon = Glyph::Sd;
  b.color = kRed;
  if (bannerCount == 1) bannerStart = now;
}

void onTouch(bool down, int16_t x, int16_t y, const UiModel& m) {
  if (down && !touching) {
    touching = true;
    dragging = false;
    downX = lastX = x;
    downY = lastY = y;
    downAt = m.now;
    scrollAtDown = setupScroll;
    return;
  }
  if (down && touching) {
    lastX = x;
    lastY = y;
    if (screen == Screen::Setup && abs(y - downY) > 8 && abs(y - downY) > abs(x - downX)) {
      dragging = true;
      setupScroll = clampf(scrollAtDown - (y - downY), -20, maxScroll() + 20);
    }
    return;
  }
  if (!down && touching) {
    touching = false;
    int dx = lastX - downX, dy = lastY - downY;
    if (dragging) {
      setupScrollTarget = clampf(setupScroll, 0, maxScroll());
      dragging = false;
    } else if (abs(dx) > 45 && abs(dx) > abs(dy) * 2) {
      onSwipe(dx < 0 ? 1 : -1, m.now);
    } else if (abs(dx) < 14 && abs(dy) < 14) {
      if (m.now - downAt > 700) onLongPress(downX, downY, m.now);
      else onTap(downX, downY, m);
    }
  }
}

void render(gfx::Surface& s, const UiModel& m) {
  dt = lastNow ? clampf((m.now - lastNow) / 1000.0f, 0, 0.1f) : 0.05f;
  lastNow = m.now;

  // idle chatter
  if (!quip || (int32_t)(m.now - quipUntil) > 0) {
    if ((int32_t)(m.now - nextIdleQuip) > 0 && companion.state(m.now) != CState::Sleeping) {
      if (m.nearbyCount) say("I sense another goblin...", m.now);
      else if (m.battPresent && !m.battCharging && m.battPct <= 15) say(pick(kLowBattQuips, 3), m.now);
      else if (m.mood == Mood::Starving || m.mood == Mood::Hungry) say(pick(kHungryQuips, 4), m.now);
      else if (m.mood == Mood::Bored) say(pick(kBoredQuips, 4), m.now);
      else if (m.mood == Mood::Happy && (rnd() & 1)) say(pick(kHappyQuips, 4), m.now);
      else say(pick(kIdleQuips, 8), m.now);
    }
  }

  uint32_t t0 = hooks.micros ? hooks.micros() : 0;
  companion.setHat(m.settings->hat);
  companion.setDroopy(m.mood == Mood::Hungry || m.mood == Mood::Starving ||
                      (m.battPresent && !m.battCharging && m.battPct <= 15));
  drawBackground(s, m);
  uint32_t t1 = hooks.micros ? hooks.micros() : 0;
  if (screen == Screen::Disclaimer || screen == Screen::Naming) {
    if (screen == Screen::Disclaimer) drawDisclaimer(s, m);
    else drawNaming(s, m);
    drawParticles(s);
    return;
  }

  // content slides in after a tab change
  float slide = 1 - easeOut((m.now - screenChangedAt) / 260.0f);
  int16_t off = (int16_t)(slide * 60 * slideDir);
  s.offset(off, 0);
  switch (screen) {
    case Screen::Home: drawHome(s, m); break;
    case Screen::Stats: drawStats(s, m); break;
    case Screen::Radar: drawRadar(s, m); break;
    case Screen::Loot: drawLoot(s, m); break;
    case Screen::Share: drawShare(s, m); break;
    case Screen::Setup: drawSetup(s, m); break;
    case Screen::TouchTest: drawTouchTest(s, m); break;
    case Screen::Disclaimer:
    case Screen::Naming: break;  // drawn full-screen above
  }
  s.offset(0, 0);
  if (slide > 0.01f) s.fillRect(0, kBodyY, W, kBodyH, kBgBottom, a8(200 * slide));
  uint32_t t2 = hooks.micros ? hooks.micros() : 0;

  drawStatusBar(s, m);
  drawNav(s);
  uint32_t t3 = hooks.micros ? hooks.micros() : 0;
  if (screen == Screen::Loot && badgeDetail >= 0) drawBadgeDetail(s, m);
  drawBanner(s, m.now);
  drawOverlay(s, m);
  drawParticles(s);
  if (hooks.micros) {
    uint32_t t4 = hooks.micros();
    prof.background = t1 - t0;
    prof.screen = t2 - t1;
    prof.chrome = t3 - t2;
    prof.overlays = t4 - t3;
  }
}

void splash(gfx::Surface& s, uint32_t now, const char* line) {
  dt = 0.05f;
  s.gradientV(0, 0, W, H, kBgTop, kBgBottom);
  float t = now / 1000.0f;
  float in = easeBack(t / 0.8f);
  s.glow(W / 2, 92, 90, kCyan, a8(70 + 30 * sinf(t * 3)));
  Companion::Look look;
  look.scale = 0.6f + 0.4f * clampf(in, 0, 1.2f);
  Companion::drawGoblin(s, W / 2, 150, now, CState::Idle, look);
  s.textCentered(fBig(), W / 2, 160, "NETWORK GOBLIN", kCyan);
  s.textCentered(fTitle(), W / 2, 188, "S  C  O  U  T", kGreen);
  // scanline sweep
  int16_t sy = (int16_t)fmodf(now / 6.0f, H);
  s.fillRect(0, sy, W, 2, kCyan, 50);
  if (line) s.textCentered(fSmall(), W / 2, 216, line, kDim);
}

}  // namespace ui
