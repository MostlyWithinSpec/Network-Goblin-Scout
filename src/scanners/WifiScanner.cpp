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
  WiFi.setBandMode(WIFI_BAND_MODE_AUTO);  // scan 2.4 GHz and 5 GHz
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
    strlcpy(s.name, WiFi.SSID(i).c_str(), sizeof(s.name));
    s.rssi = (int8_t)WiFi.RSSI(i);
    s.channel = (uint8_t)WiFi.channel(i);
    s.auth = categorize(WiFi.encryptionType(i));
    s.stableAddr = true;
    sink(s);
    seen++;
  }
  WiFi.scanDelete();
  return true;
}
