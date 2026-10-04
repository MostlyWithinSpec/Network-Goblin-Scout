#include "Achievements.h"
#include <math.h>

namespace {
bool all24(const Stats& s) {
  for (int c = 1; c <= 11; c++)  // US channels 1-11
    if (!s.channels[c]) return false;
  return true;
}
}  // namespace

// Order is the bit index in Stats::achieved (max 64). Append only.
const AchievementDef ACHIEVEMENTS[] = {
    {"first_scan", "First Scan", "Complete your first scan", [](const Stats& s) { return s.scans >= 1; }},
    {"first_network", "First Contact", "Discover a network", [](const Stats& s) { return s.wifiUnique >= 1; }},
    {"hundred_networks", "Century", "100 networks", [](const Stats& s) { return s.wifiUnique >= 100; }},
    {"thousand_networks", "Thousandaire", "1,000 networks", [](const Stats& s) { return s.wifiUnique >= 1000; }},
    {"ten_k_networks", "Goblin Horde", "10,000 networks", [](const Stats& s) { return s.wifiUnique >= 10000; }},
    {"first_enterprise", "Suit & Tie", "Find an enterprise network", [](const Stats& s) { return s.wifiEnterprise >= 1; }},
    {"signal_collector", "Signal Collector", "See a signal above -30 dBm", [](const Stats& s) { return s.bestRssi >= -30; }},
    {"five_channels", "Channel Surfer", "5 channels seen", [](const Stats& s) { return s.channels.count() >= 5; }},
    {"all_24ghz", "Full Spectrum", "All 2.4 GHz channels 1-11", all24},
    {"first_5ghz", "Fast Lane", "First 5 GHz network", [](const Stats& s) { return s.wifi5g >= 1; }},
    {"channel_collector", "Channel Collector", "20 channels seen", [](const Stats& s) { return s.channels.count() >= 20; }},
    {"first_wpa3", "Modern Lock", "Find a WPA3 network", [](const Stats& s) { return s.wifiWpa3 >= 1; }},
    {"first_ble", "Blue Tooth Fairy", "Discover a BLE device", [](const Stats& s) { return s.bleUnique >= 1; }},
    {"hundred_ble", "Beacon Hoarder", "100 BLE devices", [](const Stats& s) { return s.bleUnique >= 100; }},
    {"streak_2", "Back Again", "2-day streak", [](const Stats& s) { return s.bestStreak >= 2; }},
    {"streak_7", "Week Warrior", "7-day streak", [](const Stats& s) { return s.bestStreak >= 7; }},
    {"streak_30", "Goblin Habit", "30-day streak", [](const Stats& s) { return s.bestStreak >= 30; }},
    {"new_location", "Wanderer", "Explore a 2nd area", [](const Stats& s) { return s.geoCells >= 2; }},
    {"explorer_25", "Early Explorer", "Explore 25 areas", [](const Stats& s) { return s.geoCells >= 25; }},
};

const size_t ACHIEVEMENT_COUNT = sizeof(ACHIEVEMENTS) / sizeof(ACHIEVEMENTS[0]);
static_assert(sizeof(ACHIEVEMENTS) / sizeof(ACHIEVEMENTS[0]) <= 64, "achieved bitset is 64 wide");

namespace progression {

// Level L needs 50*(L-1)^2 XP: L10 = 4,050  L20 = 18,050  L30 = 42,050  L50 = 120,050
uint16_t levelForXp(uint32_t xp) { return (uint16_t)floor(sqrt(xp / 50.0)) + 1; }

uint32_t xpForLevel(uint16_t level) {
  uint32_t l = level > 0 ? level - 1 : 0;
  return 50UL * l * l;
}

const char* title(uint16_t level) {
  if (level >= 50) return "Goblin Architect";
  if (level >= 30) return "Signal Master";
  if (level >= 20) return "Packet Hunter";
  if (level >= 10) return "Network Wanderer";
  return "Rookie Scout";
}

}  // namespace progression
