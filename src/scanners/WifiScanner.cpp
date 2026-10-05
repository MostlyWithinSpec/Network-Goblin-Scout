#include "WifiScanner.h"
#include <WiFi.h>
#include "config.h"

namespace {
AuthCat categorize(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN:
    case WIFI_AUTH_OWE:
      return AuthCat::Open;
    case WIFI_AUTH_WEP:
      return AuthCat::WEP;
    case WIFI_AUTH_WPA_PSK:
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK:
      return AuthCat::WPA;
    case WIFI_AUTH_WPA3_PSK:
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return AuthCat::WPA3;
    case WIFI_AUTH_WPA2_ENTERPRISE:
    case WIFI_AUTH_WPA3_ENTERPRISE:
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
      return AuthCat::Enterprise;
    default:
      return AuthCat::Other;
  }
}
}  // namespace

bool WifiScanner::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
#if CONFIG_SOC_WIFI_SUPPORT_5G
  // Scan 2.4 GHz and 5 GHz. Must run after WiFi.mode() has started the STA interface.
  if (!WiFi.setBandMode(WIFI_BAND_MODE_AUTO)) log_w("wifi: dual-band mode failed, 2.4 GHz only");
#endif
  return true;
}

void WifiScanner::start() {
  // async=true, show_hidden=true, passive=true
  WiFi.scanNetworks(true, true, true, WIFI_SCAN_MS_PER_CHAN);
  running_ = true;
}

bool WifiScanner::poll(void (*sink)(const Sighting&), uint32_t& seen) {
  if (!running_) return true;
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return false;
  running_ = false;
  seen = 0;
  if (n < 0) {  // WIFI_SCAN_FAILED
    log_w("wifi: scan failed");
    return true;
  }
  for (int16_t i = 0; i < n; i++) {
    Sighting s{};
    s.radio = Radio::WiFi;
    const uint8_t* b = WiFi.BSSID(i);
    if (!b) continue;
    memcpy(s.mac, b, 6);
    s.macLen = 6;
    strlcpy(s.name, WiFi.SSID(i).c_str(), sizeof(s.name));
    s.rssi = (int8_t)WiFi.RSSI(i);
    s.channel = (uint8_t)WiFi.channel(i);
    s.auth = categorize(WiFi.encryptionType(i));
    s.stableAddr = true;
    s.panId = 0xFFFF;
    if (!s.name[0]) s.flags |= sflag::kHidden;
    if (const auto* rec = (const wifi_ap_record_t*)WiFi.getScanInfoByIndex(i)) {
      if (rec->phy_11ax) s.flags |= sflag::kWifi6;
      if (rec->wps) s.flags |= sflag::kWps;
    }
    if (s.name[0]) remember(s.name, s.rssi, s.auth == AuthCat::Open);
    sink(s);
    seen++;
  }
  WiFi.scanDelete();
  return true;
}

void WifiScanner::remember(const char* ssid, int8_t rssi, bool open) {
  uint32_t now = millis();
  WifiChoice* slot = &recent_[0];
  for (auto& r : recent_) {
    if (r.seenMs && strcmp(r.ssid, ssid) == 0) { slot = &r; break; }
    if (r.seenMs < slot->seenMs) slot = &r;  // else replace the stalest
  }
  if (strcmp(slot->ssid, ssid) != 0 || now - slot->seenMs > 10000 || rssi > slot->rssi) slot->rssi = rssi;
  strlcpy(slot->ssid, ssid, sizeof(slot->ssid));
  slot->open = open;
  slot->seenMs = now ? now : 1;
}

size_t WifiScanner::recent(WifiChoice* out, size_t max) const {
  size_t n = 0;
  uint32_t now = millis();
  for (const auto& r : recent_)
    if (r.seenMs && now - r.seenMs < 120000 && n < max) out[n++] = r;
  for (size_t i = 1; i < n; i++)  // strongest first
    for (size_t j = i; j > 0 && out[j].rssi > out[j - 1].rssi; j--) {
      WifiChoice t = out[j];
      out[j] = out[j - 1];
      out[j - 1] = t;
    }
  return n;
}
