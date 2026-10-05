#include "BleScanner.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "../social/PeerCodec.h"
#include "config.h"

// BLE callbacks run on the BLE host task, so they must NOT touch SPI (display/SD).
// They only copy into a queue; the main loop drains it in poll().
namespace {
QueueHandle_t q = nullptr;
volatile bool scanDone = false;
BLEScan* scan = nullptr;

bool parseMac(const char* str, uint8_t out[6]) {
  unsigned v[6];
  if (sscanf(str, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)v[i];
  return true;
}

// Walks the raw advertising payload (AD structures: len, type, data...).
// Sets beacon flags, and decodes an NG Scout goblin beacon if there is one.
uint8_t inspect(const uint8_t* p, size_t len, peercodec::Info& peerOut, bool& isPeer) {
  uint8_t flags = 0;
  isPeer = false;
  for (size_t i = 0; i + 1 < len;) {
    uint8_t n = p[i];
    if (n == 0 || i + 1 + n > len) break;
    uint8_t type = p[i + 1];
    const uint8_t* d = p + i + 2;
    size_t dl = n - 1;
    if (type == 0xFF) {  // manufacturer specific
      if (dl >= 4 && d[0] == 0x4C && d[1] == 0x00 && d[2] == 0x02 && d[3] == 0x15) flags |= sflag::kIBeacon;
      if (peercodec::decode(d, dl, peerOut)) isPeer = true;
    } else if ((type == 0x16 || type == 0x03 || type == 0x02) && dl >= 2 && d[0] == 0xAA && d[1] == 0xFE) {
      flags |= sflag::kEddystone;  // 16-bit service UUID 0xFEAA
    } else if (type == 0x08 || type == 0x09) {
      flags |= sflag::kNamed;
    }
    i += 1 + n;
  }
  return flags;
}

class Callbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    Sighting s{};
    if (!parseMac(d.getAddress().toString().c_str(), s.mac)) return;
    s.macLen = 6;
    s.rssi = (int8_t)d.getRSSI();
    s.panId = 0xFFFF;
    peercodec::Info peer;
    bool isPeer = false;
    s.flags = inspect(d.getPayload(), d.getPayloadLength(), peer, isPeer);
    if (isPeer) {
      // Another NG Scout. Identity is its goblin id, not the (random, per-boot) address.
      s.radio = Radio::Peer;
      memset(s.mac, 0, sizeof(s.mac));
      memcpy(s.mac, &peer.id, 4);
      s.macLen = 4;
      s.peerLevel = peer.level;
      s.peerHue = peer.hue;
      s.flags = peer.flags & 0x1F;  // equipped hat id
      strlcpy(s.name, peer.name, sizeof(s.name));
      xQueueSend(q, &s, 0);
      return;
    }
    s.radio = Radio::BLE;
    if (d.haveName()) strlcpy(s.name, d.getName().c_str(), sizeof(s.name));
    // Address type 0 = public. For random addresses the top two bits of the
    // most significant byte are 0b11 for "static random" (stable until reboot of
    // the device); anything else is a rotating private address.
    uint8_t type = (uint8_t)d.getAddressType();
    s.stableAddr = (type == 0) || ((s.mac[0] & 0xC0) == 0xC0);
    xQueueSend(q, &s, 0);  // drop if full — it's fine to miss a few
  }
};
Callbacks callbacks;

void onScanComplete(BLEScanResults) { scanDone = true; }
}  // namespace

bool BleScanner::begin() {
  q = xQueueCreate(64, sizeof(Sighting));
  BLEDevice::init("");
  scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(&callbacks, false /* no duplicates */);
  scan->setActiveScan(false);  // passive
  scan->setInterval(100);
  scan->setWindow(90);
  ready_ = (q != nullptr && scan != nullptr);
  return ready_;
}

void BleScanner::start() {
  if (!ready_) { scanDone = true; return; }
  scanDone = false;
  cycleSeen_ = 0;
  scan->clearResults();
  scan->start(BLE_SCAN_SECONDS, onScanComplete, false);
}

bool BleScanner::poll(void (*sink)(const Sighting&), uint32_t& seen) {
  Sighting s;
  while (q && xQueueReceive(q, &s, 0) == pdTRUE) {
    sink(s);
    cycleSeen_++;
  }
  seen = cycleSeen_;
  if (!scanDone) return false;
  scan->clearResults();
  return true;
}
