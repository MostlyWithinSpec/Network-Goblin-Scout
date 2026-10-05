#include "Peer.h"
#include <BLEDevice.h>
#include <Preferences.h>
#include <esp_random.h>

namespace {
peercodec::Info me;
bool advertising = false;
bool addrSet = false;
bool isNamed = false;

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

void begin(uint32_t savedId, const char* savedName) {
  Preferences prefs;
  prefs.begin("ngscout", false);
  uint32_t nvsId = prefs.isKey("peerId") ? prefs.getUInt("peerId", 0) : 0;
  uint32_t id = savedId ? savedId : nvsId;
  if (!id) {
    do id = esp_random(); while (!id);
  }
  if (nvsId != id) prefs.putUInt("peerId", id);
  String stored = prefs.isKey("name") ? prefs.getString("name", "") : String();
  String name = (savedName && savedName[0]) ? String(savedName) : stored;
  if (name.length() && stored != name) prefs.putString("name", name);
  prefs.end();
  me.id = id;
  me.hue = (uint8_t)(id >> 24);
  isNamed = name.length() > 0;
  if (isNamed) strlcpy(me.name, name.c_str(), sizeof(me.name));
  else peercodec::nameFor(id, me.name, sizeof(me.name));  // placeholder until named
  log_i("peer: I am %s%s", me.name, isNamed ? "" : " (not named yet)");
}

bool named() { return isNamed; }

void setName(const char* name) {
  strlcpy(me.name, name, sizeof(me.name));
  isNamed = me.name[0] != 0;
  Preferences prefs;
  prefs.begin("ngscout", false);
  prefs.putString("name", me.name);
  prefs.end();
  if (advertising) advertise();
}

bool agreedNvs() {
  Preferences prefs;
  prefs.begin("ngscout", false);  // read-write: read-only fails (and logs) if the namespace is new
  bool a = prefs.isKey("agreed") && prefs.getBool("agreed", false);
  prefs.end();
  return a;
}

void setAgreed() {
  Preferences prefs;
  prefs.begin("ngscout", false);
  prefs.putBool("agreed", true);
  prefs.end();
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

void setHat(uint8_t hat) {
  uint8_t f = (me.flags & 0xE0) | (hat & 0x1F);
  if (f == me.flags) return;
  me.flags = f;
  if (advertising) advertise();
}

void setHoardTier(uint8_t tier) {
  uint8_t f = (uint8_t)((me.flags & 0x1F) | (tier & 7) << 5);
  if (f == me.flags) return;
  me.flags = f;
  if (advertising) advertise();
}

bool beaconOn() { return advertising; }

}  // namespace peer
