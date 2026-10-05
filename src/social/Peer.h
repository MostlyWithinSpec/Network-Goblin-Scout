#pragma once
#include <Arduino.h>
#include "PeerCodec.h"

// This goblin's identity and its "I'm a goblin" BLE beacon, so two Scouts notice each
// other (like a Pwnagotchi). The only thing NG Scout ever transmits. The beacon carries
// a random goblin id, generated name, level and colour; it is non-connectable and uses
// a random address made at each boot, never the chip's real Bluetooth MAC.
namespace peer {
void begin();                       // load or create identity (NVS: works without SD)
const peercodec::Info& self();
// Call after BLEDevice::init() (BleScanner::begin does that).
void startBeacon(uint16_t level);
void stopBeacon();
void update(uint16_t level);        // cheap; only re-advertises when the level changes
void setHat(uint8_t hat);           // shown to other goblins; re-advertises if changed
bool beaconOn();
}  // namespace peer
