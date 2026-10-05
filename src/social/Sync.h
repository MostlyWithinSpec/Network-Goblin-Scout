#pragma once
#include <Arduino.h>

class ScanManager;

// Goblin Sync: when the owner presses "Sync now", join the owner's Wi-Fi, upload a summary of
// counts to the leaderboard (scout.networkgoblin.dev, see the network-goblin-labs repo api/),
// disconnect and go back to passive scanning. Never uploads SSIDs, MACs, ids or locations.
//
// Identity: a random 32-byte secret made on this device. Its public "key" is the first 8 bytes
// of SHA-256(secret); uploads are HMAC-signed with the secret, and the very first one carries it
// so the server can register the key. Kept in NVS and mirrored to /scout/sync.key on the SD card
// (a factory flash wipes NVS; without the card copy the goblin would lose its leaderboard entry).
namespace goblinsync {

enum class State : uint8_t { Idle, Waiting, Connecting, Uploading, Done, Failed };

struct Status {
  State state = State::Idle;
  char msg[48] = "";       // what happened, for the screen
  char motd[80] = "";      // server's message of the day
  uint32_t rank = 0, of = 0;
  uint32_t atMs = 0;       // when the last sync finished (0 = never this session)
};

void begin();                                   // after storage + engine + peer
bool hasWifi();
const char* ssid();                             // "" if not set
void setWifi(const char* ssid, const char* pass);
const char* key();                              // 16 hex chars
const char* profileUrl();                       // the goblin's public page
void start();                                   // owner pressed Sync
void tick(ScanManager& scans);                  // from loop(): drives the steps
bool busy();
const Status& status();

}  // namespace goblinsync
