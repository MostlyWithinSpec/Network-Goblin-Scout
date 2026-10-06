#include "BleScanner.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "../core/Trackers.h"
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

bool hasUuid16(const uint8_t* d, size_t dl, uint16_t uuid) {  // list of 16-bit service UUIDs
  for (size_t k = 0; k + 1 < dl; k += 2)
    if ((uint16_t)(d[k] | d[k + 1] << 8) == uuid) return true;
  return false;
}

// Walks the raw advertising payload (AD structures: len, type, data...).
// Sets beacon flags, spots item trackers, and decodes an NG Scout goblin beacon if there is one.
//
// Tracker signatures (as used by tracker-detection apps such as AirGuard):
//   Apple Find My, *separated from its owner*: manufacturer 0x004C, type 0x12, length 0x19
//     (near its owner it sends a short 0x12 0x02 form, which we ignore)
//   Samsung SmartTag: service data 0xFD5A    Tile: service 0xFEED / 0xFEEC
//   Google Find My Device: Eddystone service data 0xFEAA with frame type 0x40 / 0x41
//   Chipolo: service 0xFE33
//
// For loot (core/Loot.h) it also notes the first manufacturer data's company id and the byte after
// it, Google Fast Pair service data (0xFE2C) and Flipper Zero's service UUIDs (0x3081-0x3083).
uint8_t inspect(const uint8_t* p, size_t len, peercodec::Info& peerOut, bool& isPeer, uint8_t& tracker,
                uint16_t& company, uint8_t& msgType) {
  uint8_t flags = 0;
  isPeer = false;
  tracker = trackers::kNone;
  company = 0;
  msgType = 0;
  for (size_t i = 0; i + 1 < len;) {
    uint8_t n = p[i];
    if (n == 0 || i + 1 + n > len) break;
    uint8_t type = p[i + 1];
    const uint8_t* d = p + i + 2;
    size_t dl = n - 1;
    if ((type == 0x02 || type == 0x03) &&
        (hasUuid16(d, dl, 0x3081) || hasUuid16(d, dl, 0x3082) || hasUuid16(d, dl, 0x3083)))
      flags |= sflag::kFlipper;
    if (type == 0x16 && dl >= 2 && d[0] == 0x2C && d[1] == 0xFE) flags |= sflag::kFastPair;
    if (type == 0xFF && dl >= 2 && !(flags & sflag::kCompany)) {
      flags |= sflag::kCompany;
      company = (uint16_t)(d[0] | d[1] << 8);
      msgType = dl >= 3 ? d[2] : 0;
    }
    if (type == 0xFF) {  // manufacturer specific
      if (dl >= 4 && d[0] == 0x4C && d[1] == 0x00 && d[2] == 0x02 && d[3] == 0x15) flags |= sflag::kIBeacon;
      if (dl >= 4 && d[0] == 0x4C && d[1] == 0x00 && d[2] == 0x12 && d[3] == 0x19) tracker = trackers::kFindMy;
      if (peercodec::decode(d, dl, peerOut)) isPeer = true;
    } else if (type == 0x16 && dl >= 3 && d[0] == 0xAA && d[1] == 0xFE && (d[2] == 0x40 || d[2] == 0x41)) {
      tracker = trackers::kGoogle;
    } else if (type == 0x16 && dl >= 2 && d[0] == 0x5A && d[1] == 0xFD) {
      tracker = trackers::kSmartTag;
    } else if ((type == 0x16 || type == 0x03 || type == 0x02) && dl >= 2 && d[0] == 0xAA && d[1] == 0xFE) {
      flags |= sflag::kEddystone;  // 16-bit service UUID 0xFEAA
    } else if ((type == 0x03 || type == 0x02) && (hasUuid16(d, dl, 0xFEED) || hasUuid16(d, dl, 0xFEEC))) {
      tracker = trackers::kTile;
    } else if (type == 0x16 && dl >= 2 && ((d[0] == 0xED && d[1] == 0xFE) || (d[0] == 0xEC && d[1] == 0xFE))) {
      tracker = trackers::kTile;
    } else if ((type == 0x03 || type == 0x02) && hasUuid16(d, dl, 0xFE33)) {
      tracker = trackers::kChipolo;
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
    uint8_t tracker = 0;
    s.flags = inspect(d.getPayload(), d.getPayloadLength(), peer, isPeer, tracker, s.company, s.msgType);
    if (isPeer) {
      // Another NG Scout. Identity is its goblin id, not the (random, per-boot) address.
      s.radio = Radio::Peer;
      memset(s.mac, 0, sizeof(s.mac));
      memcpy(s.mac, &peer.id, 4);
      s.macLen = 4;
      s.peerLevel = peer.level;
      s.peerHue = peer.hue;
      s.flags = peer.flags & 0x1F;  // equipped hat id
      s.peerHoard = peer.flags >> 5;
      strlcpy(s.name, peer.name, sizeof(s.name));
      xQueueSend(q, &s, 0);
      return;
    }
    s.radio = Radio::BLE;
    s.tracker = tracker;
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
