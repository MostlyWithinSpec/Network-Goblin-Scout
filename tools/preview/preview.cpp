// Renders the real UI code on a PC into PNG screenshots (via PPM + tools/preview/run.sh).
// No hardware needed: the model is filled with made-up stats.
#include <cstdio>
#include <utility>
#include <vector>
#include "core/Achievements.h"
#include "core/Hats.h"
#include "core/Quests.h"
#include "ui/Ui.h"

static uint16_t fb[320 * 240], bg[320 * 240];
static gfx::Surface surf(fb, 320, 240);
static Stats stats;
static Settings settings;
static UiModel model;
static uint32_t now = 1000;
static NearbyPeer peer;
static QuestBoard board;
static Blip blips[48];

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
  model.battPresent = true;
  model.battPct = 72;
  model.fwVersion = "0.3.0";
  stats.questsDone = 4;
  stats.hunger = 380;
  stats.boredom = 610;
  settings.hat = 8;  // crown
  model.mood = Mood::Content;
  model.hunger = (uint16_t)stats.hunger;
  model.boredom = (uint16_t)stats.boredom;
  quests::deal(stats, model.level, 7, board);
  board.q[0].base -= 3;                       // some progress
  board.q[1].base -= board.q[1].target;       // one finished
  board.q[1].done = true;
  model.quests = &board;
  model.uptimeMin = 900;
  model.hatMask = hats::unlockedMask(stats, model.level) | 0x1FF;
  model.shareUrl = "https://github.com/MostlyWithinSpec/Network-Goblin-Scout";
  uint32_t seed = 99;
  for (auto& b : blips) {
    seed = seed * 1103515245 + 12345;
    b.angle = (seed >> 8) & 4095;
    b.rssi = (int8_t)(-35 - (int)((seed >> 20) % 60));
    int r = (seed >> 4) % 10;
    b.radio = r < 5 ? Radio::WiFi : r < 8 ? Radio::BLE : r < 9 ? Radio::Thread : Radio::Peer;
    b.lastSeenMs = 1000;
  }
  model.blips = blips;
  model.blipCount = 48;
  peer.id = 1; strcpy(peer.name, "Grimwick"); peer.level = 12; peer.hue = 200;

  ui::begin(ui::Hooks(), bg);

  model.now = now;
  ui::splash(surf, 900, "warming up radios...");
  save("01_splash");

  // first run: disclaimer, then type a name on the keyboard
  ui::startOnboarding(true, true);
  run(600);
  save("01b_disclaimer");
  tap(160, 216);                                   // I UNDERSTAND
  run(200);
  for (auto k : {std::make_pair(146, 132), {113, 96}, {206, 96}, {192, 170}}) tap(k.first, k.second);  // G R U B
  run(300);
  save("01c_naming");
  tap(270, 212);                                   // DONE
  run(400);

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
  for (int i : {0, 1, 2, 3, 5, 9, 12, 19, 28, 41}) ui::onEvent(ev(EventType::Achievement, i, "x"), now);
  run(3500 * 2 + 1600);  // two single cards play, then the batch
  save("06b_achievement_batch");
  run(3000);

  UiEvent pe = ev(EventType::PeerNew, 12, "Met Grimwick!");
  strcpy(pe.peerName, "Grimwick"); pe.peerLevel = 12; pe.peerHue = 200; pe.peerHat = 9;
  ui::onEvent(pe, now);
  run(2200);
  save("07_encounter");
  run(4000);

  tap(80, 150);  // pet
  run(400);
  save("08_home_pet");

  for (auto& b : blips) b.lastSeenMs = now;
  ui::debugShow(1, 0); run(1200); save("09_radar");
  ui::debugShow(2, 0); run(600); save("10_stats_spectrum");
  ui::debugShow(2, 1); run(600); save("11_stats_records");
  ui::debugShow(3, 0); run(600); save("12_loot_quests");
  ui::debugShow(3, 1); run(600); save("13_loot_wardrobe");
  ui::debugShow(3, 2); run(600); save("14_loot_trophies");
  ui::debugBadge(0); run(300); save("15_badge_detail");
  ui::debugBadge(-1);
  ui::debugShow(4, 0); run(600); save("16_setup");
  ui::debugShow(4, 200); run(600); save("17_setup_scrolled");
  ui::debugShow(6, 0); run(600); save("18_share_card");
  ui::onEvent(ev(EventType::HatUnlocked, 4, "New hat: Propeller Cap!"), now);
  ui::debugShow(0, 0);
  run(1400); save("19_new_hat");
  run(3000);
  ui::debugShow(0, 0);
  model.scanning = nullptr;
  ui::pet().setBase(CState::Sleeping);
  run(2000);
  save("20_home_sleeping");
  printf("rendered\n");
  return 0;
}
