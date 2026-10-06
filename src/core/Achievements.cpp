#include "Achievements.h"
#include <math.h>

namespace {
bool all24(const Stats& s) {
  for (int c = 1; c <= 11; c++)  // US channels 1-11
    if (!s.channels[c]) return false;
  return true;
}
bool unii1(const Stats& s) { return s.channels[36] && s.channels[40] && s.channels[44] && s.channels[48]; }
uint16_t lvl(const Stats& s) { return progression::levelForXp(s.xp); }
size_t badges(const Stats& s) { return s.achieved.count(); }
}  // namespace

#define A(id, name, desc, tier, icon, secret, expr) \
  {id, name, desc, tier, Icon::icon, secret, [](const Stats& s) -> bool { return (expr); }}

// Order is the bit index in Stats::achieved (max 256). Append only; never reorder or
// rename ids (they're saved on SD). Names and descriptions can be edited freely.
const AchievementDef ACHIEVEMENTS[] = {
    // ---- v0.1 originals (ids 0-18) ----------------------------------------------
    A("first_scan", "First Scan", "Complete your first scan", kBronze, Signal, false, s.scans >= 1),
    A("first_network", "First Contact", "Discover a network", kBronze, Wifi, false, s.wifiUnique >= 1),
    A("hundred_networks", "Century", "100 networks", kSilver, Wifi, false, s.wifiUnique >= 100),
    A("thousand_networks", "Thousandaire", "1,000 networks", kGold, Wifi, false, s.wifiUnique >= 1000),
    A("ten_k_networks", "Goblin Horde", "10,000 networks", kLegendary, Wifi, false, s.wifiUnique >= 10000),
    A("first_enterprise", "Suit & Tie", "Find an enterprise network", kBronze, Wifi, false, s.wifiEnterprise >= 1),
    A("signal_collector", "Signal Collector", "See a signal above -30 dBm", kBronze, Signal, false, s.bestRssi >= -30),
    A("five_channels", "Channel Surfer", "5 Wi-Fi channels seen", kBronze, Signal, false, s.channels.count() >= 5),
    {"all_24ghz", "Full Spectrum", "All 2.4 GHz channels 1-11", kSilver, Icon::Signal, false, all24},
    A("first_5ghz", "Fast Lane", "First 5 GHz network", kBronze, Wifi, false, s.wifi5g >= 1),
    A("channel_collector", "Channel Collector", "20 Wi-Fi channels seen", kGold, Signal, false, s.channels.count() >= 20),
    A("first_wpa3", "Modern Lock", "Find a WPA3 network", kBronze, Wifi, false, s.wifiWpa3 >= 1),
    A("first_ble", "Blue Tooth Fairy", "Discover a BLE device", kBronze, Ble, false, s.bleUnique >= 1),
    A("hundred_ble", "Beacon Hoarder", "100 BLE devices", kSilver, Ble, false, s.bleUnique >= 100),
    A("streak_2", "Back Again", "2-day streak (GPS)", kBronze, Clock, false, s.bestStreak >= 2),
    A("streak_7", "Week Warrior", "7-day streak (GPS)", kSilver, Clock, false, s.bestStreak >= 7),
    A("streak_30", "Goblin Habit", "30-day streak (GPS)", kGold, Clock, false, s.bestStreak >= 30),
    A("new_location", "Wanderer", "Explore a 2nd area (GPS)", kBronze, Map, false, s.geoCells >= 2),
    A("explorer_25", "Early Explorer", "Explore 25 areas (GPS)", kSilver, Map, false, s.geoCells >= 25),

    // ---- Wi-Fi volume ------------------------------------------------------------
    A("ten_networks", "Getting Started", "10 networks", kBronze, Wifi, false, s.wifiUnique >= 10),
    A("fifty_networks", "Neighbourhood Watch", "50 networks", kBronze, Wifi, false, s.wifiUnique >= 50),
    A("five_hundred_networks", "Half a Grand", "500 networks", kSilver, Wifi, false, s.wifiUnique >= 500),
    A("five_k_networks", "Signal Sponge", "5,000 networks", kGold, Wifi, false, s.wifiUnique >= 5000),
    A("twenty_five_k_networks", "The Great Hoard", "25,000 networks", kLegendary, Wifi, false, s.wifiUnique >= 25000),
    A("ssid_100", "Name Collector", "100 unique network names", kSilver, Wifi, false, s.ssidUnique >= 100),
    A("ssid_1000", "Encyclopedia", "1,000 unique network names", kGold, Wifi, false, s.ssidUnique >= 1000),

    // ---- Wi-Fi flavours ----------------------------------------------------------
    A("fivegig_50", "Five Gee Whiz", "50 networks on 5 GHz", kSilver, Wifi, false, s.wifi5g >= 50),
    A("fivegig_500", "Band Leader", "500 networks on 5 GHz", kGold, Wifi, false, s.wifi5g >= 500),
    A("first_wifi6", "Sixth Sense", "Spot a Wi-Fi 6 router", kBronze, Wifi, false, s.wifi6 >= 1),
    A("wifi6_100", "Future Proof", "100 Wi-Fi 6 routers", kGold, Wifi, false, s.wifi6 >= 100),
    A("first_wps", "Push the Button", "A router with WPS", kBronze, Wifi, false, s.wifiWps >= 1),
    A("first_hidden", "Ghost Hunter", "Detect a hidden network", kBronze, Wifi, false, s.wifiHidden >= 1),
    A("hidden_50", "Phantom Menace", "50 hidden networks", kGold, Wifi, false, s.wifiHidden >= 50),
    A("open_25", "Wide Open Spaces", "25 open networks", kSilver, Wifi, false, s.wifiOpen >= 25),
    A("first_wep", "Archaeologist", "Find a WEP network. In this century?!", kSilver, Wifi, false, s.wifiWep >= 1),
    A("wpa3_50", "Locksmith", "50 WPA3 networks", kGold, Wifi, false, s.wifiWpa3 >= 50),
    A("enterprise_25", "Corporate Ladder", "25 enterprise networks", kSilver, Wifi, false, s.wifiEnterprise >= 25),
    A("first_dfs", "Radar Dodger", "Network on a DFS channel (52-144)", kSilver, Signal, false, s.wifiDfs >= 1),
    A("channel_165", "Edge of the Band", "Hear channel 165", kSilver, Signal, false, s.channels[165]),
    {"unii1_set", "Low Band Set", "Channels 36, 40, 44 and 48", kSilver, Icon::Signal, false, unii1},
    A("thirty_channels", "Spectrum Hog", "30 Wi-Fi channels seen", kLegendary, Signal, false, s.channels.count() >= 30),

    // ---- Busy air & signal -------------------------------------------------------
    A("crowded_25", "Busy Air", "25 networks in a single scan", kBronze, Signal, false, s.maxApsInScan >= 25),
    A("crowded_50", "Signal Soup", "50 networks in a single scan", kSilver, Signal, false, s.maxApsInScan >= 50),
    A("crowded_100", "Packet Storm", "100 networks in a single scan", kGold, Signal, false, s.maxApsInScan >= 100),
    A("whisper", "Whisper Listener", "Hear a network at -92 dBm or weaker", kSilver, Signal, false, s.worstRssi && s.worstRssi <= -92),
    A("point_blank", "Point Blank", "A signal above -20 dBm", kSilver, Signal, false, s.bestRssi >= -20),

    // ---- Bluetooth ---------------------------------------------------------------
    A("ble_10", "Blue Streak", "10 BLE devices", kBronze, Ble, false, s.bleUnique >= 10),
    A("ble_500", "Bluetooth Baron", "500 BLE devices", kGold, Ble, false, s.bleUnique >= 500),
    A("ble_2000", "Blue Monarch", "2,000 BLE devices", kLegendary, Ble, false, s.bleUnique >= 2000),
    A("ble_sightings_1000", "Busy Bees", "1,000 BLE sightings", kBronze, Ble, false, s.bleSightings >= 1000),
    A("ble_sightings_100k", "Hive of Activity", "100,000 BLE sightings", kGold, Ble, false, s.bleSightings >= 100000),
    A("named_ble_50", "Name Dropper", "50 BLE devices that say their name", kSilver, Ble, false, s.bleNamed >= 50),
    A("first_ibeacon", "Beacon Spotter", "Find an iBeacon", kBronze, Ble, false, s.bleIBeacon >= 1),
    A("first_eddystone", "Eddy Current", "Find an Eddystone beacon", kSilver, Ble, false, s.bleEddystone >= 1),
    A("ble_crowd_50", "Bluetooth Blizzard", "50 BLE devices in one scan", kSilver, Ble, false, s.maxBleInScan >= 50),
    A("ble_crowd_150", "Concert Mode", "150 BLE devices in one scan", kLegendary, Ble, false, s.maxBleInScan >= 150),

    // ---- 802.15.4 (Zigbee / Thread) ----------------------------------------------
    A("first_154", "Low Power Lurker", "Hear an 802.15.4 device", kBronze, Mesh, false, s.t154Unique >= 1),
    A("first_zigbee", "Bee Whisperer", "Find a Zigbee network", kSilver, Mesh, false, s.zigbeePans >= 1),
    A("first_thread", "Common Thread", "Find a Thread network", kSilver, Mesh, false, s.threadPans >= 1),
    A("both_meshes", "Mesh Bilingual", "Find both Zigbee and Thread", kGold, Mesh, false, s.zigbeePans && s.threadPans),
    A("pans_5", "Mesh Head", "5 mesh networks", kSilver, Mesh, false, s.t154Pans >= 5),
    A("pans_25", "Mesh Overlord", "25 mesh networks", kGold, Mesh, false, s.t154Pans >= 25),
    A("devices154_50", "Smart Home Spotter", "50 mesh devices", kSilver, Mesh, false, s.t154Unique >= 50),
    A("devices154_500", "Internet of Goblins", "500 mesh devices", kLegendary, Mesh, false, s.t154Unique >= 500),
    A("frames154_1000", "Chatterbox", "Overhear 1,000 mesh frames", kBronze, Mesh, false, s.t154Frames >= 1000),
    A("mesh_channels_4", "Hopscotch", "Mesh traffic on 4 channels", kSilver, Mesh, false, s.channels154.count() >= 4),

    // ---- Other goblins -----------------------------------------------------------
    A("first_peer", "Not Alone", "Meet another goblin", kSilver, Goblin, false, s.peersMet >= 1),
    A("peers_5", "Goblin Gang", "Meet 5 goblins", kGold, Goblin, false, s.peersMet >= 5),
    A("peers_25", "Horde Leader", "Meet 25 goblins", kLegendary, Goblin, false, s.peersMet >= 25),
    A("reunion", "Old Friends", "Meet a goblin you've met before", kSilver, Goblin, false, s.peerEncounters > s.peersMet),
    A("encounters_50", "Social Butterfly", "50 goblin encounters", kGold, Goblin, false, s.peerEncounters >= 50),
    A("party_3", "Goblin Party", "3 goblins nearby at once", kGold, Goblin, false, s.maxPeersAtOnce >= 3),
    A("party_5", "Goblin Rave", "5 goblins nearby at once", kLegendary, Goblin, false, s.maxPeersAtOnce >= 5),
    A("met_higher", "Senpai Noticed Me", "Meet a higher-level goblin", kSilver, Goblin, false, s.metHigherLevel),
    A("close_encounter", "Close Encounter", "Get right next to another goblin", kBronze, Goblin, false, s.closeEncounter),

    // ---- Levels & XP -------------------------------------------------------------
    A("level_5", "Getting Warm", "Reach level 5", kBronze, Star, false, lvl(s) >= 5),
    A("level_10", "Double Digits", "Reach level 10", kSilver, Star, false, lvl(s) >= 10),
    A("level_20", "Packet Pro", "Reach level 20", kSilver, Star, false, lvl(s) >= 20),
    A("level_30", "Signal Sage", "Reach level 30", kGold, Star, false, lvl(s) >= 30),
    A("level_50", "Goblin Architect", "Reach level 50", kLegendary, Star, false, lvl(s) >= 50),
    A("level_100", "Mythic Goblin", "Reach level 100", kLegendary, Star, true, lvl(s) >= 100),
    A("xp_1000", "Shiny Hoard", "1,000 XP", kBronze, Star, false, s.xp >= 1000),
    A("xp_10k", "Dragon's Hoard", "10,000 XP", kSilver, Star, false, s.xp >= 10000),
    A("xp_100k", "Goblin Treasury", "100,000 XP", kGold, Star, false, s.xp >= 100000),

    // ---- Dedication --------------------------------------------------------------
    A("scans_100", "Persistent", "100 scans", kBronze, Signal, false, s.scans >= 100),
    A("scans_1000", "Dedicated", "1,000 scans", kSilver, Signal, false, s.scans >= 1000),
    A("scans_10000", "Obsessed", "10,000 scans", kGold, Signal, false, s.scans >= 10000),
    A("scans_100k", "Never Sleeps", "100,000 scans", kLegendary, Signal, false, s.scans >= 100000),
    A("sessions_10", "Regular", "Power on 10 times", kBronze, Clock, false, s.sessions >= 10),
    A("sessions_100", "Lifer", "Power on 100 times", kSilver, Clock, false, s.sessions >= 100),
    A("uptime_1h", "Hour of Power", "1 hour of scanning", kBronze, Clock, false, s.uptimeMin >= 60),
    A("uptime_24h", "Day Tripper", "24 hours of scanning", kSilver, Clock, false, s.uptimeMin >= 1440),
    A("uptime_7d", "Week Walker", "7 days of scanning", kGold, Clock, false, s.uptimeMin >= 10080),
    A("uptime_30d", "Marathon Goblin", "30 days of scanning", kLegendary, Clock, false, s.uptimeMin >= 43200),
    A("streak_100", "Unstoppable", "100-day streak (GPS)", kLegendary, Clock, false, s.bestStreak >= 100),
    A("explorer_10", "Road Trip", "Explore 10 areas (GPS)", kBronze, Map, false, s.geoCells >= 10),
    A("explorer_100", "Cartographer", "Explore 100 areas (GPS)", kGold, Map, false, s.geoCells >= 100),
    A("explorer_500", "Globetrotter", "Explore 500 areas (GPS)", kLegendary, Map, false, s.geoCells >= 500),

    // ---- The goblin itself -------------------------------------------------------
    A("pet_1", "Head Pat", "Pet your goblin", kBronze, Paw, false, s.pets >= 1),
    A("pet_100", "Goblin Whisperer", "Pet your goblin 100 times", kSilver, Paw, false, s.pets >= 100),
    A("pet_1000", "Too Many Pats", "Seriously, 1,000 pats", kGold, Paw, true, s.pets >= 1000),

    // ---- Combos, meta and secrets ------------------------------------------------
    A("triple_threat", "Triple Threat", "Wi-Fi, Bluetooth and mesh devices found", kSilver, Trophy, false,
      s.wifiUnique && s.bleUnique && s.t154Unique),
    A("omniscient", "Omniscient", "Wi-Fi, BLE, mesh AND another goblin", kGold, Trophy, false,
      s.wifiUnique && s.bleUnique && s.t154Unique && s.peersMet),
    A("ach_10", "Collector", "Unlock 10 achievements", kBronze, Trophy, false, badges(s) >= 10),
    A("ach_25", "Trophy Case", "Unlock 25 achievements", kSilver, Trophy, false, badges(s) >= 25),
    A("ach_50", "Completionist-ish", "Unlock 50 achievements", kGold, Trophy, false, badges(s) >= 50),
    A("ach_90", "Achievement Goblin", "Unlock 90 achievements", kLegendary, Trophy, false, badges(s) >= 90),
    A("jackpot", "Jackpot", "Have exactly 777 networks", kSilver, Lock, true, s.wifiUnique == 777),
    A("leet", "1337 h4x0r", "Have exactly 1,337 networks", kGold, Lock, true, s.wifiUnique == 1337),
    A("nice_signal", "Nice", "Strongest signal exactly -69 dBm", kBronze, Lock, true, s.bestRssi == -69),

    // ---- v0.4: battery, trackers, sniff-offs, day/night, seasons (ids 110-121) --------
    A("unplugged", "Unplugged", "An hour on battery power", kBronze, Signal, false, s.batteryMin >= 60),
    A("road_warrior", "Road Warrior", "10 hours on battery power", kSilver, Map, false, s.batteryMin >= 600),
    A("tag_spotter", "Tag Spotter", "Hear an item tracker (AirTag, Tile...)", kBronze, Ble, false, s.trackersSeen >= 1),
    A("tag_collector", "Tag Collector", "Hear 100 item trackers", kSilver, Ble, false, s.trackersSeen >= 100),
    A("watchful", "Watchful Goblin", "Get a tracker alert", kSilver, Star, false, s.trackerAlerts >= 1),
    A("first_sniff", "First Sniff", "Have a sniff-off with another goblin", kBronze, Goblin, false, s.sniffOffs >= 1),
    A("top_nose", "Top Nose", "Win 10 sniff-offs", kSilver, Goblin, false, s.sniffWins >= 10),
    A("hoard_champion", "Hoard Champion", "Win 50 sniff-offs", kGold, Goblin, false, s.sniffWins >= 50),
    A("night_owl", "Night Owl", "Scout for an hour after 9 pm", kBronze, Clock, false, s.nightMin >= 60),
    A("insomniac", "Insomniac", "20 hours of night scouting", kSilver, Clock, true, s.nightMin >= 1200),
    A("festive", "Festive", "Earn a seasonal hat", kBronze, Star, false, s.seasonMask != 0),
    A("four_seasons", "Four Seasons", "Earn all four seasonal hats", kGold, Star, false, s.seasonMask == 15),

    // ---- v0.5: Goblin Sync (id 122) ------------------------------------------------
    A("first_sync", "Online Goblin", "Sync your goblin to the leaderboard", kBronze, Trophy, false, s.syncs >= 1),

    // ---- v0.6: loot rarity (ids 123-132) ---------------------------------------------
    A("loot_rare", "Shiny!", "Find a Rare network", kBronze, Star, false, s.lootRare + s.lootEpic + s.lootLegendary >= 1),
    A("loot_epic", "Jackpot", "Find an Epic network", kSilver, Star, false, s.lootEpic + s.lootLegendary >= 1),
    A("loot_legendary", "Legend Hunter", "Find a Legendary network", kGold, Star, false, s.lootLegendary >= 1),
    A("loot_legendary_10", "Mythmaker", "Find 10 Legendary networks", kLegendary, Star, false, s.lootLegendary >= 10),
    A("brands_10", "Window Shopper", "Collect 10 brands in the Hoard Book", kBronze, Trophy, false, s.lootBrands >= 10),
    A("brands_40", "Brand Collector", "Collect 40 brands", kSilver, Trophy, false, s.lootBrands >= 40),
    A("brands_80", "Hoard Book Scholar", "Collect 80 brands", kGold, Trophy, false, s.lootBrands >= 80),
    A("all_kinds", "Full Shelf", "Find every kind of loot", kLegendary, Trophy, false,
      s.lootKinds == (1u << loot::K_COUNT) - 1),
    A("space_goblin", "Space Goblin", "Find a Starlink dish", kSilver, Wifi, true, s.lootKinds & (1u << loot::K_SAT)),
    A("joyride", "Joyride", "Find a car's Wi-Fi", kSilver, Wifi, true, s.lootKinds & (1u << loot::K_CAR)),

    // ---- v0.6.1: Bluetooth loot and hacker gear (ids 133-141) -----------------------------
    A("audiophile", "Audiophile", "Find some headphones or a speaker", kBronze, Ble, false,
      s.lootKinds & (1u << loot::K_AUDIO)),
    A("wrist_watcher", "Wrist Watcher", "Find a smartwatch or fitness band", kBronze, Ble, false,
      s.lootKinds & (1u << loot::K_WEARABLE)),
    A("dolphin_spotter", "Dolphin Spotter", "Spot a Flipper Zero", kSilver, Star, true,
      s.hackerMask & (1u << loot::H_FLIPPER)),
    A("pwnagotchi_pal", "Pwnagotchi Pal", "Spot a Pwnagotchi", kGold, Star, true, s.hackerMask & (1u << loot::H_PWNAGOTCHI)),
    A("fruit_salad", "Fruit Salad", "Spot a Wi-Fi Pineapple", kGold, Star, true, s.hackerMask & (1u << loot::H_PINEAPPLE)),
    A("script_kiddie", "Script Kiddie Detector", "Spot an ESP deauther", kSilver, Star, true,
      s.hackerMask & (1u << loot::H_DEAUTHER)),
    A("popup_survivor", "Popup Survivor", "Live through a Bluetooth popup spam storm", kSilver, Ble, true,
      s.hackerMask & (1u << loot::H_BLESPAM)),
    A("hacker_bingo", "Hacker Bingo", "Spot all five kinds of hacker gear", kLegendary, Trophy, true,
      s.hackerMask == (1u << loot::H_COUNT) - 1),
    A("con_season", "Con Season", "Spot hacker gear 25 times", kGold, Trophy, false, s.hackerSpots >= 25),

    // ---- v0.6.2: SquachWatch visits (ids 142-143) ------------------------------------------
    A("bigfoot_sighting", "Bigfoot Sighting", "Get a visit from a SquachWatch", kGold, Paw, true, s.squachVisits >= 1),
    A("squad_goals", "Squad Goals", "10 SquachWatch visits", kLegendary, Paw, true, s.squachVisits >= 10),
};

#undef A

const size_t ACHIEVEMENT_COUNT = sizeof(ACHIEVEMENTS) / sizeof(ACHIEVEMENTS[0]);
static_assert(sizeof(ACHIEVEMENTS) / sizeof(ACHIEVEMENTS[0]) <= 256, "achieved bitset is 256 wide");

namespace progression {

// Level L needs 50*(L-1)^2 XP: L10 = 4,050  L20 = 18,050  L30 = 42,050  L50 = 120,050
uint16_t levelForXp(uint32_t xp) { return (uint16_t)floor(sqrt(xp / 50.0)) + 1; }

uint32_t xpForLevel(uint16_t level) {
  uint32_t l = level > 0 ? level - 1 : 0;
  return 50UL * l * l;
}

const char* title(uint16_t level) {
  if (level >= 100) return "Mythic Goblin";
  if (level >= 50) return "Goblin Architect";
  if (level >= 40) return "Spectrum Warlock";
  if (level >= 30) return "Signal Master";
  if (level >= 20) return "Packet Hunter";
  if (level >= 15) return "Mesh Prowler";
  if (level >= 10) return "Network Wanderer";
  if (level >= 5) return "Antenna Apprentice";
  return "Rookie Scout";
}

}  // namespace progression
