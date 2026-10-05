#pragma once
#include <Arduino.h>
#include <bitset>

// Values are mixed into stored ids: never renumber. (Thread = any 802.15.4, incl. Zigbee;
// its tag is also used for exploration-cell ids.)
enum class Radio : uint8_t { WiFi = 1, BLE = 2, Thread = 3, Peer = 4 };

enum class AuthCat : uint8_t { Open, WEP, WPA, WPA3, Enterprise, Other };

// Sighting::flags bits, per radio
namespace sflag {
// Wi-Fi
const uint8_t kWifi6 = 0x01;     // 802.11ax
const uint8_t kWps = 0x02;
const uint8_t kHidden = 0x04;    // empty SSID
// BLE
const uint8_t kIBeacon = 0x01;
const uint8_t kEddystone = 0x02;
const uint8_t kNamed = 0x04;     // advertises a local name
// 802.15.4
const uint8_t kZigbee = 0x01;    // looks like Zigbee (NWK header / beacon protocol 0)
const uint8_t kThread = 0x02;    // looks like Thread (6LoWPAN / MAC security / beacon protocol 3)
const uint8_t kBeacon = 0x04;    // MAC beacon frame
}  // namespace sflag

// A network for the Setup > Sync Wi-Fi picker. RAM only (never saved or logged: design rule 1);
// only the network the owner picks is kept, in NVS, to sync over.
struct WifiChoice {
  char ssid[33];
  int8_t rssi;
  bool open;
  uint32_t seenMs;
};

// One observation of a transmitter, produced by a scanner module.
struct Sighting {
  Radio radio;
  uint8_t mac[8];     // address (Wi-Fi/BLE: 6 bytes, 802.15.4: 2 or 8, Peer: goblin id)
  uint8_t macLen;
  char name[33];      // SSID / BLE local name / peer goblin name (may be empty)
  int8_t rssi;
  uint8_t channel;    // Wi-Fi channel, 802.15.4 channel 11-26, 0 for BLE
  AuthCat auth;       // Wi-Fi only
  bool stableAddr;    // BLE: public or static-random (false = rotating private addr)
  uint8_t flags;      // sflag:: bits for this radio
  uint16_t panId;     // 802.15.4 PAN id (0xFFFF = none)
  uint16_t peerLevel; // Peer only
  uint8_t peerHue;    // Peer only: colour of their goblin
  uint8_t tracker;    // BLE only: trackers::Kind if it looks like an item tracker (0 = no)
  uint8_t peerHoard;  // Peer only: hoard tier 0-7 (social/Sniff.h)
};

// Lifetime counters saved in state.json under their own names. Add new ones at will;
// never rename (the name is the JSON key). Counters derived from the SD id stores
// (unique networks/devices/areas/goblins) live in Stats below instead.
#define NG_SAVED_COUNTERS(X)                                                              \
  X(xp) X(scans) X(sessions) X(pets) X(uptimeMin)                                         \
  X(wifi5g) X(wifiOpen) X(wifiWep) X(wifiWpa3) X(wifiEnterprise) X(wifi6) X(wifiWps)      \
  X(wifiHidden) X(wifiDfs) X(maxApsInScan)                                                \
  X(bleSightings) X(bleNamed) X(bleIBeacon) X(bleEddystone) X(maxBleInScan)               \
  X(t154Frames) X(zigbeePans) X(threadPans)                                               \
  X(peerEncounters) X(maxPeersAtOnce) X(metHigherLevel) X(closeEncounter)                 \
  X(lastDay) X(streak) X(bestStreak)                                                     \
  X(questsDone) X(boardsCleared) X(hunger) X(boredom)                                    \
  X(batteryMin) X(trackersSeen) X(trackerAlerts) X(sniffOffs) X(sniffWins)                     \
  X(nightMin) X(seasonMask) X(syncs)

struct Stats {
#define NG_DECLARE_COUNTER(n) uint32_t n = 0;
  NG_SAVED_COUNTERS(NG_DECLARE_COUNTER)
#undef NG_DECLARE_COUNTER

  // Derived from the id stores on SD at boot
  uint32_t wifiUnique = 0;      // unique BSSIDs
  uint32_t ssidUnique = 0;      // unique non-hidden SSIDs
  uint32_t bleUnique = 0;       // stable-address BLE devices only
  uint32_t t154Unique = 0;      // unique 802.15.4 devices
  uint32_t t154Pans = 0;        // unique 802.15.4 networks (PAN id + channel)
  uint32_t peersMet = 0;        // unique goblins
  uint32_t geoCells = 0;        // GPS exploration cells

  int8_t bestRssi = -127;       // strongest Wi-Fi signal ever
  int8_t worstRssi = 0;         // weakest Wi-Fi signal ever (0 = none yet)
  std::bitset<200> channels;    // Wi-Fi channels seen, indexed by channel number
  std::bitset<27> channels154;  // 802.15.4 channels seen (11-26)
  std::bitset<128> achieved;

  // This power-on session (not persisted)
  uint32_t sessWifiNew = 0;
  uint32_t sessBleNew = 0;
  uint32_t sess154New = 0;
  uint32_t sessXp = 0;
  uint32_t lastScanSeen = 0;    // APs in the most recent Wi-Fi scan
  uint32_t lastBleSeen = 0;     // advertisers in the most recent BLE scan
  uint32_t last154Seen = 0;     // frames in the most recent 802.15.4 sweep
};

struct Settings {
  uint8_t brightness = 80;
  bool bleScan = true;
  bool scan154 = true;          // passive Zigbee/Thread listening
  bool beacon = true;           // advertise "I'm a goblin" so other Scouts notice us
  bool gps = true;
  bool sound = true;
  bool invert = false;
  bool fastDisplay = true;      // "Turbo display": SPI at full crystal speed
  uint8_t hat = 0;              // equipped hat id (0 = none), see core/Hats.h
  uint32_t goblinId = 0;        // copy of the NVS identity: survives a factory flash (which wipes NVS)
  String goblinName = "";       // chosen by the owner at first boot ("" = not named yet)
  bool agreed = false;          // first-run disclaimer accepted
  int16_t tzMin = 0;            // local time = GPS UTC + this (set from Setup > Clock)
  String spritePack = "goblin";
};

enum class EventType : uint8_t {
  NewWifi, NewBle, NewChannel, LevelUp, Achievement, NewCell, DailyBonus,
  New154, PeerNew, PeerReunion, QuestDone, BoardCleared, NewQuests, HatUnlocked, TrackerAlert, SniffOff, Synced
};

struct UiEvent {
  EventType type;
  uint32_t value;
  char text[40];
  // Peer events: the other goblin
  char peerName[13];
  uint16_t peerLevel;
  uint8_t peerHue;
  uint8_t peerHat;
  uint8_t peerHoard;
};

// A goblin seen recently (for the status bar and the encounter scene).
struct NearbyPeer {
  uint32_t id = 0;
  char name[13] = {0};
  uint16_t level = 0;
  uint8_t hue = 0;
  int8_t rssi = -127;
  uint8_t hat = 0;
  uint8_t hoard = 0;         // hoard tier (social/Sniff.h)
  uint32_t lastSeenMs = 0;
};

// ---- Quests ---------------------------------------------------------------
// A quest is "grow metric X by `target` since the board was dealt" (see core/Quests.h).
struct Quest {
  uint8_t type = 0;     // QuestType; saved, append-only
  uint16_t target = 0;
  uint32_t base = 0;    // metric value when dealt
  bool done = false;
};

struct QuestBoard {
  Quest q[3];
  bool active = false;      // false = waiting for a new board
  uint32_t nextAtMin = 0;   // uptimeMin when the next board is dealt
};

// ---- Needs -----------------------------------------------------------------
// hunger / boredom are 0..1000 saved counters; the mood is derived from them.
enum class Mood : uint8_t { Happy, Content, Hungry, Bored, Starving };

// ---- Radar -----------------------------------------------------------------
// Something heard recently, for the radar screen. `angle` comes from the salted id,
// so the radar never sees an address; it just keeps each device in the same spot.
struct Blip {
  uint16_t angle = 0;   // 0..4095
  int8_t rssi = -127;
  Radio radio = Radio::WiFi;
  uint32_t lastSeenMs = 0;
};
