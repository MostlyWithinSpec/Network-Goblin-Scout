// Renders the real UI code on a PC into PNG screenshots (via PPM + tools/preview/run.sh).
// No hardware needed: the model is filled with made-up stats.
#include <cstdio>
#include <utility>
#include <vector>
#include "core/Achievements.h"
#include "core/Clock.h"
#include "core/Hats.h"
#include "core/Quests.h"
#include "core/Trackers.h"
#include "social/Sniff.h"
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
  // Hoard Book: a spread of finds
  stats.lootCommon = 812; stats.lootUncommon = 141; stats.lootRare = 23; stats.lootEpic = 3; stats.lootLegendary = 1;
  for (const char* k : {"mystery", "odd", "netgear", "tplink", "arris", "sagemcom", "eero", "google", "meraki", "cisco",
                        "aruba", "hp", "epson", "hotspot", "direct", "amazon", "sonos", "roku", "nintendo", "tesla",
                        "ring", "espressif", "starlink", "garmin", "bose", "tilebt", "flipper"}) {
    uint16_t b = loot::brandByKey(k);
    stats.lootBrand[b] = 3 + (b * 37) % 90;
    stats.lootBrands++;
    stats.lootKinds |= 1u << loot::kBrands[b].kind;
  }
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
  // pets (Setup > Pet)
  settings.pet = 1; run(1200); save("02b_pet_pip");
  settings.pet = 2; run(1200); save("02c_pet_lily");
  settings.pet = 3; run(1200); save("02d_pet_both");
  tap(80, 150); run(500); save("02e_pet_purr");
  ui::pet().setBase(CState::Sleeping); run(2500); save("02f_pet_both_asleep");
  ui::pet().setBase(CState::Idle);
  ui::debugShow(4, 220); run(800); save("02g_setup_pet_row");
  ui::debugShow(0, 0);
  run(3000);
  settings.pet = 0;

  model.scanning = "Wi-Fi";
  ui::pet().setBase(CState::Scanning);
  model.nearby[0] = &peer;
  model.nearbyCount = 1;
  ui::onEvent(ev(EventType::NewWifi, 3, "3 new! Best: eero"), now);
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
  model.hatMask = 0x3FFFF;  // all hats, to see the art
  ui::debugShow(3, 1); run(600); save("13_loot_wardrobe");
  model.hatMask = hats::unlockedMask(stats, model.level) | 0x1FF;
  ui::debugShow(3, 2); run(600); save("13b_loot_hoard_book");
  ui::debugShow(3, 3); run(600); save("14_loot_trophies");
  ui::debugBadge(0); run(300); save("15_badge_detail");
  ui::debugBadge(-1);
  ui::debugShow(4, 0); run(600); save("16_setup");
  ui::debugShow(4, 200); run(600); save("17_setup_scrolled");
  ui::debugShow(6, 0); run(600); save("18_share_card");
  ui::onEvent(ev(EventType::HatUnlocked, 4, "New hat: Propeller Cap!"), now);
  ui::debugShow(0, 0);
  run(1400); save("19_new_hat");
  run(3000);
  ui::onEvent(ev(EventType::LootFind, loot::R_EPIC | loot::K_SAT << 8 | 20u << 16, "Starlink"), now);
  run(900); save("19b_loot_epic");
  run(3000);
  ui::onEvent(ev(EventType::LootFind, loot::R_LEGENDARY | loot::K_BIZ << 8 | 50u << 16, "Cisco Meraki"), now);
  run(900); save("19c_loot_legendary");
  run(3000);
  ui::onEvent(ev(EventType::LootFind, loot::R_RARE | loot::K_WEARABLE << 8 | 8u << 16 | 1u << 24, "Oura ring"), now);
  run(900); save("19d_loot_ble_rare");
  run(3000);
  {
    const char* names[] = {"flipper", "pwnagotchi", "pineapple", "deauther", "blespam"};
    for (uint32_t h = 0; h < loot::H_COUNT; h++) {
      ui::onEvent(ev(EventType::Hacker, h | 30u << 16, loot::hackerName((uint8_t)h)), now);
      run(h == 0 ? 900 : 1500);
      char name[40];
      snprintf(name, sizeof(name), "19e_hacker_%u_%s", (unsigned)h, names[h]);
      save(name);
      run(5000);
    }
  }
  ui::onEvent(ev(EventType::Squach, 0u | 40u << 16, "Bigfoot"), now);
  run(1800); save("19f_squach_highfive");
  run(1500); save("19g_squach_hangout");
  run(4000);
  ui::onEvent(ev(EventType::Squach, 1u | 40u << 16, ""), now);
  run(2600); save("19h_squach_legend");
  run(5000);
  ui::debugShow(0, 0);
  model.scanning = nullptr;
  ui::pet().setBase(CState::Sleeping);
  run(2000);
  save("20_home_sleeping");
  ui::pet().setBase(CState::Idle);

  // tracker alert: an AirTag that came along through 4 places over 23 minutes
  ui::onEvent(ev(EventType::TrackerAlert, trackers::kFindMy | 4u << 8 | 23u << 16, "Tracker following you!"), now);
  run(900); save("21_tracker_alert");
  tap(240, 214);  // GOT IT
  run(400);
  model.trackersNearby = 2;
  ui::debugShow(1, 0); run(600); save("22_radar_trackers");
  model.trackersNearby = 0;

  // sniff-off with Grimwick: our "Big hoard" (tier 4) beats their "Modest pile" (tier 2)
  {
    UiEvent e = ev(EventType::SniffOff, sniff::kWin | 4u << 8 | 2u << 16 | 60u << 24, "Sniff-off won! +60 XP");
    snprintf(e.peerName, sizeof(e.peerName), "Grimwick");
    e.peerLevel = 12; e.peerHue = 200; e.peerHat = 3;
    ui::debugShow(0, 0);
    ui::onEvent(e, now);
    run(1300); save("23_sniff_off");
    run(1700); save("24_sniff_result");
    run(3000);
  }
  // day/night: dusk on Home, then night (the goblin naps; stars come out), then set the clock
  model.buildTime = clk::toUnix({2026, 10, 5, 13, 29});
  model.localTime = clk::toUnix({2026, 10, 31, 19, 15});
  ui::debugShow(0, 0); run(1500); save("25_home_dusk");
  model.localTime = clk::toUnix({2026, 10, 31, 23, 40});
  ui::pet().setBase(CState::Sleeping);
  run(1500); save("26_home_night");
  ui::pet().setBase(CState::Idle);
  model.localTime = 0;
  ui::debugShow(4, 400); run(600); save("27_setup_clock_row");
  ui::debugShow(9, 0); run(600); save("28_set_clock");
  // Goblin Sync: not set up, Wi-Fi picker, password keyboard (symbols), syncing, synced
  static WifiChoice nets[] = {{"GoblinCave", -48, false, 1}, {"Cafe Free WiFi", -63, true, 1},
                              {"NETGEAR-5G", -71, false, 1}, {"xfinitywifi", -80, true, 1}};
  model.nets = nets;
  model.netCount = 4;
  model.syncProfile = "https://scout.networkgoblin.dev/g/?k=3f9a1c22b07e4d51";
  ui::debugShow(10, 0); run(600); save("29_sync_new");
  tap(100, 64);  // Wi-Fi row -> picker
  run(600); save("30_wifi_pick");
  tap(100, 50);  // GoblinCave -> password keyboard
  for (auto k : {std::make_pair(146, 132), {37, 96}, {68, 96}, {99, 96}}) tap(k.first, k.second);  // g o b l? (letters)
  tap(43, 212);  // 123
  for (auto k : {std::make_pair(21, 96), {52, 96}, {83, 96}, {84, 170}}) tap(k.first, k.second);  // 1 2 3 !
  run(300); save("31_keyboard_symbols");
  for (auto k : {std::make_pair(21, 96), {52, 96}, {83, 96}}) tap(k.first, k.second);
  tap(270, 212);  // DONE
  run(400);
  model.syncSsid = "GoblinCave";
  model.sync = UiModel::Sync::Busy;
  model.syncMsg = "Uploading the hoard...";
  run(800); save("32_syncing");
  model.sync = UiModel::Sync::Done;
  model.syncRegistered = true;
  model.syncMsg = "Synced! #12 of 340";
  model.syncMotd = "Rumour has it there's a goblin with 10,000 networks. Is it you?";
  ui::onEvent(ev(EventType::Synced, 12, "Synced! #12 of 340"), now);
  run(900); save("33_synced");
  model.sync = UiModel::Sync::Failed;
  model.syncMsg = "Wi-Fi refused: wrong password?";
  model.syncMotd = "";
  run(3000); save("34_sync_failed");
  tap(266, 192);  // "remove me" once: asks to confirm
  run(300); save("35_remove_confirm");
  printf("rendered\n");
  return 0;
}
