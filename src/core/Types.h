#pragma once
#include <Arduino.h>
#include <bitset>

enum class Radio : uint8_t { WiFi = 1, BLE = 2, Thread = 3 };

enum class AuthCat : uint8_t { Open, WEP, WPA, WPA3, Enterprise, Other };

// One observation of a transmitter, produced by a scanner module.
struct Sighting {
  Radio radio;
  uint8_t mac[6];
  char name[33];      // SSID / BLE local name (may be empty)
  int8_t rssi;
  uint8_t channel;    // Wi-Fi channel (0 for BLE)
  AuthCat auth;       // Wi-Fi only
  bool stableAddr;    // BLE: public or static-random (false = rotating private addr)
};

struct Stats {
  uint32_t xp = 0;
  uint32_t scans = 0;
  uint32_t sessions = 0;

  // Wi-Fi
  uint32_t wifiUnique = 0;      // unique BSSIDs
  uint32_t ssidUnique = 0;      // unique non-hidden SSIDs
  uint32_t wifi5g = 0;
  uint32_t wifiOpen = 0;
  uint32_t wifiWpa3 = 0;
  uint32_t wifiEnterprise = 0;
  int8_t bestRssi = -127;
  std::bitset<200> channels;    // indexed by channel number

  // BLE
  uint32_t bleUnique = 0;       // stable-address devices only
  uint32_t bleSightings = 0;    // everything incl. rotating addresses

  // Exploration (needs GPS)
  uint32_t geoCells = 0;
  uint32_t lastDay = 0;         // days since epoch (UTC)
  uint32_t streak = 0;
  uint32_t bestStreak = 0;

  std::bitset<64> achieved;

  // This power-on session (not persisted)
  uint32_t sessWifiNew = 0;
  uint32_t sessBleNew = 0;
  uint32_t sessXp = 0;
  uint32_t lastScanSeen = 0;    // APs in the most recent scan
};

struct Settings {
  uint8_t brightness = 80;
  bool bleScan = true;
  bool gps = true;
  bool sound = true;
  bool invert = false;
  String spritePack = "goblin";
};

enum class EventType : uint8_t { NewWifi, NewBle, NewChannel, LevelUp, Achievement, NewCell, DailyBonus };

struct UiEvent {
  EventType type;
  uint32_t value;
  char text[40];
};
