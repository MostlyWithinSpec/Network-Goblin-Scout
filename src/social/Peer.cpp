#include "Peer.h"
#include <BLEDevice.h>
#include <Preferences.h>
#include <esp_random.h>

namespace {
peercodec::Info me;
bool advertising = false;
bool addrSet = false;

void advertise() {
  uint8_t payload[peercodec::kMaxLen];
  size_t n = peercodec::encode(me, payload);
  BLEAdvertisementData data;
  data.setFlags(0x04);  // BR/EDR not supported, not discoverable/connectable
  data.setManufacturerData(String((const char*)payload, n));

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->stop();
  adv->setAdvertisementType(BLE_GAP_CONN_MODE_NON);  // nobody can connect to us
  adv->setScanResponse(false);
  adv->setMinInterval(0x0320);  // 500 ms (units of 0.625 ms)
  adv->setMaxInterval(0x0480);  // 720 ms
  adv->setAdvertisementData(data);
  advertising = adv->start();
  if (!advertising) log_w("peer: beacon failed to start");
}
}  // namespace

namespace peer {

void begin() {
  Preferences prefs;
  prefs.begin("ngscout", false);
  uint32_t id = prefs.getUInt("peerId", 0);
  if (!id) {
    do id = esp_random(); while (!id);
    prefs.putUInt("peerId", id);
  }
  prefs.end();
  me.id = id;
  me.hue = (uint8_t)(id >> 24);
  peercodec::nameFor(id, me.name, sizeof(me.name));
  log_i("peer: I am %s", me.name);
}

const peercodec::Info& self() { return me; }

void startBeacon(uint16_t level) {
  if (!addrSet) {
    // Random static address (top two bits 11), new every boot. NimBLE byte order: [5] is MSB.
    uint8_t addr[6];
    esp_fill_random(addr, sizeof(addr));
    addr[5] |= 0xC0;
    addrSet = BLEDevice::setOwnAddr(addr) && BLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
    if (!addrSet) { log_w("peer: could not set a random address, beacon stays off"); return; }
  }
  me.level = level;
  advertise();
}

void stopBeacon() {
  if (advertising) BLEDevice::getAdvertising()->stop();
  advertising = false;
}

void update(uint16_t level) {
  if (level == me.level) return;
  me.level = level;
  if (advertising) advertise();
}

bool beaconOn() { return advertising; }

}  // namespace peer
