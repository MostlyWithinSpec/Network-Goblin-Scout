#include "BleScanner.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
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

class Callbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    Sighting s{};
    s.radio = Radio::BLE;
    if (!parseMac(d.getAddress().toString().c_str(), s.mac)) return;
    s.rssi = (int8_t)d.getRSSI();
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
