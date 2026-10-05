#include "ThreadScanner.h"
#include <esp_ieee802154.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "Ieee802154Frame.h"
#include "config.h"

// The receive callback runs in interrupt context: it only copies the frame into a
// queue and hands the driver its buffer back. Parsing happens in poll() on the main loop.
namespace {
struct RawFrame {
  uint8_t len;      // PSDU length without FCS
  int8_t rssi;
  uint8_t channel;
  uint8_t data[125];
};
QueueHandle_t q = nullptr;
volatile uint32_t dropped = 0;
volatile bool listening = false;
}  // namespace

// Overrides the driver's weak default (which just releases the buffer).
extern "C" void IRAM_ATTR esp_ieee802154_receive_done(uint8_t* frame, esp_ieee802154_frame_info_t* info) {
  // frame[0] is the PHY length byte (PSDU incl. 2-byte FCS), the PSDU follows.
  uint8_t len = frame[0];
  if (listening && q && len > 2 && len <= 127) {
    RawFrame f;
    f.len = len - 2;
    f.rssi = info->rssi;
    f.channel = info->channel;
    memcpy(f.data, frame + 1, f.len);
    BaseType_t woken = pdFALSE;
    if (xQueueSendFromISR(q, &f, &woken) != pdTRUE) dropped = dropped + 1;
    esp_ieee802154_receive_handle_done(frame);
    if (woken) portYIELD_FROM_ISR();
    return;
  }
  esp_ieee802154_receive_handle_done(frame);
}

bool ThreadScanner::begin() {
  q = xQueueCreate(32, sizeof(RawFrame));
  ready_ = q != nullptr;
  return ready_;
}

void ThreadScanner::tune(uint8_t ch) {
  channel_ = ch;
  esp_ieee802154_set_channel(ch);
  esp_ieee802154_receive();
  hopAt_ = millis() + IEEE802154_DWELL_MS;
}

void ThreadScanner::start() {
  cycleSeen_ = 0;
  running_ = false;
  if (!ready_) return;
  if (esp_ieee802154_enable() != ESP_OK) {
    log_w("802.15.4: enable failed");
    return;
  }
  esp_ieee802154_set_promiscuous(true);   // accept frames for any PAN/address
  esp_ieee802154_set_rx_when_idle(true);  // keep listening between frames
  listening = true;
  running_ = true;
  tune(11);
}

void ThreadScanner::finish() {
  listening = false;
  esp_ieee802154_set_rx_when_idle(false);
  esp_ieee802154_disable();
  running_ = false;
  if (dropped) {
    log_w("802.15.4: %lu frames dropped (queue full)", (unsigned long)dropped);
    dropped = 0;
  }
}

bool ThreadScanner::poll(void (*sink)(const Sighting&), uint32_t& seen) {
  RawFrame f;
  while (q && xQueueReceive(q, &f, 0) == pdTRUE) {
    ieee802154::Frame h;
    if (!ieee802154::parse(f.data, f.len, h) || !h.srcLen) continue;  // ACKs etc. carry no address
    Sighting s{};
    s.radio = Radio::Thread;
    s.channel = f.channel ? f.channel : channel_;
    s.rssi = f.rssi;
    s.panId = h.pan();
    s.stableAddr = true;
    if (h.srcLen == 8) {
      memcpy(s.mac, h.src, 8);
      s.macLen = 8;
    } else {  // short addresses are only unique within their PAN
      s.mac[0] = s.panId & 0xFF;
      s.mac[1] = s.panId >> 8;
      memcpy(s.mac + 2, h.src, 2);
      s.macLen = 4;
    }
    if (h.proto == ieee802154::kZigbee) s.flags |= sflag::kZigbee;
    if (h.proto == ieee802154::kThread) s.flags |= sflag::kThread;
    if (h.type == ieee802154::kBeacon) s.flags |= sflag::kBeacon;
    sink(s);
    cycleSeen_++;
  }
  seen = cycleSeen_;
  if (!running_) return true;
  if ((int32_t)(millis() - hopAt_) < 0) return false;
  if (channel_ < 26) {
    tune(channel_ + 1);
    return false;
  }
  finish();
  return true;
}
