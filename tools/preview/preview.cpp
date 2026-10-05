// Renders the real UI code on a PC into PNG screenshots (via PPM + tools/preview/run.sh).
// No hardware needed: the model is filled with made-up stats.
#include <cstdio>
#include <vector>
#include "core/Achievements.h"
#include "ui/Ui.h"

static uint16_t fb[320 * 240], bg[320 * 240];
static gfx::Surface surf(fb, 320, 240);
static Stats stats;
static Settings settings;
static UiModel model;
static uint32_t now = 1000;
static NearbyPeer peer;

static void save(const char* name) {
  char path[128];
  snprintf(path, sizeof(path), "%s.ppm", name);
  FILE* f = fopen(path, "wb");
  fprintf(f, "P6\n320 240\n255\n");
  for (int i = 0; i < 320 * 240; i++) {
    uint16_t c = fb[i];
    unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3 | (c >> 13)), (unsigned char)(((c >> 5) & 0x3F) << 2 | ((c >> 9) & 3)),
                            (unsigned char)((c & 0x1F) << 3 | ((c >> 2) & 7))};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

static void run(uint32_t ms) {  // advance time, rendering ~25 fps like the device
  for (uint32_t t = 0; t < ms; t += 40) {
    now += 40;
    model.now = now;
    ui::render(surf, model);
  }
}

static void tap(int16_t x, int16_t y) {
  ui::onTouch(true, x, y, model);
  ui::onTouch(false, x, y, model);
}

static UiEvent ev(EventType t, uint32_t v, const char* text) {
  UiEvent e{};
  e.type = t;
  e.value = v;
  snprintf(e.text, sizeof(e.text), "%s", text);
  return e;
}

int main() {
  stats.xp = 5230;
  stats.wifiUnique = 1342; stats.ssidUnique = 988; stats.bleUnique = 412; stats.t154Unique = 37;
  stats.peersMet = 3; stats.peerEncounters = 7; stats.t154Pans = 4; stats.zigbeePans = 3; stats.threadPans = 1;
  stats.wifi5g = 420; stats.wifiOpen = 61; stats.wifiWep = 2; stats.wifiWpa3 = 140; stats.wifiEnterprise = 58;
  stats.wifi6 = 210; stats.wifiWps = 77; stats.wifiHidden = 33; stats.maxApsInScan = 64; stats.maxBleInScan = 48;
  stats.bestRssi = -24; stats.worstRssi = -94; stats.scans = 4821; stats.sessions = 31; stats.uptimeMin = 2900;
  stats.bleIBeacon = 4; stats.geoCells = 6; stats.bestStreak = 5; stats.pets = 12;
  stats.sessWifiNew = 23; stats.sessBleNew = 9; stats.sess154New = 2; stats.sessXp = 156;
  for (int c : {1, 2, 3, 4, 6, 8, 11, 36, 40, 44, 48, 52, 100, 112, 149, 157, 161}) stats.channels.set(c);
  for (int c : {11, 15, 20, 25}) stats.channels154.set(c);
  for (int i = 0; i < 37; i++) stats.achieved.set((i * 7) % ACHIEVEMENT_COUNT);
  model.stats = &stats;
  model.settings = &settings;
  model.level = progression::levelForXp(stats.xp);
  model.xpLo = progression::xpForLevel(model.level);
  model.xpHi = progression::xpForLevel(model.level + 1);
  model.sd = true;
  model.gpsPresent = true;
  model.gpsFix = true;
  model.sats = 7;
  model.myName = "Snagpacket";
  model.myId = 0x5A1D2B3C;
  model.beaconOn = true;
  model.fwVersion = "0.2.0";
  peer.id = 1; strcpy(peer.name, "Grimwick"); peer.level = 12; peer.hue = 200;

  ui::begin(ui::Hooks(), bg);

  model.now = now;
  ui::splash(surf, 900, "warming up radios...");
  save("01_splash");

  ui::pet().setBase(CState::Idle);
  run(3000);
  save("02_home_idle");

  model.scanning = "Wi-Fi";
  ui::pet().setBase(CState::Scanning);
  model.nearby[0] = &peer;
  model.nearbyCount = 1;
  ui::onEvent(ev(EventType::NewWifi, 3, "3 new networks!"), now);
  run(700);
  save("03_home_scanning_banner");
  run(3000);

  model.scanning = "802.15.4";
  ui::onEvent(ev(EventType::New154, 4, "New Zigbee network!"), now);
  run(500);
  save("04_home_mesh");
  run(3000);
  model.nearbyCount = 0;

  ui::onEvent(ev(EventType::LevelUp, 11, "Level 11!"), now);
  run(1400);
  save("05_levelup");
  run(3000);

  ui::onEvent(ev(EventType::Achievement, 67, "Not Alone"), now);
  run(1300);
  save("06_achievement");
  run(3000);

  UiEvent pe = ev(EventType::PeerNew, 12, "Met Grimwick!");
  strcpy(pe.peerName, "Grimwick"); pe.peerLevel = 12; pe.peerHue = 200;
  ui::onEvent(pe, now);
  run(2200);
  save("07_encounter");
  run(4000);

  tap(80, 150);  // pet
  run(400);
  save("08_home_pet");

  ui::debugShow(1, 0); run(600); save("09_stats_spectrum");
  ui::debugShow(1, 1); run(600); save("10_stats_records");
  ui::debugShow(2, 0); run(600); save("11_badges");
  ui::debugBadge(0); run(300); save("12_badge_detail");
  ui::debugBadge(-1);
  ui::debugShow(2, 4); run(300); save("13_badges_page5");
  ui::debugShow(3, 0); run(600); save("14_setup");
  ui::debugShow(3, 160); run(600); save("15_setup_scrolled");

  ui::debugShow(0, 0);
  model.scanning = nullptr;
  ui::pet().setBase(CState::Sleeping);
  run(2000);
  save("16_home_sleeping");
  printf("rendered\n");
  return 0;
}
