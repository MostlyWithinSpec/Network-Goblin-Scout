#include "Sync.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_random.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>
#include "../core/Achievements.h"
#include "../core/Engine.h"
#include "../hal/Storage.h"
#include "../scanners/ScanManager.h"
#include "Peer.h"
#include "config.h"

namespace {
const char* kNs = "ngsync";
const char* kKeyFile = "/scout/sync.key";

char ssid_[33] = "";
char pass_[64] = "";
uint8_t secret_[32];
char secretHex_[65] = "";
char key_[17] = "";
char profile_[96] = "";
uint32_t seq_ = 0;
bool claimed_ = false;
goblinsync::Status status_;
uint32_t waitSince_ = 0;
bool forgetting_ = false;  // this job removes our leaderboard entry instead of syncing

// The upload runs in its own task so the UI keeps animating while Wi-Fi and TLS do their thing.
// It never touches SPI (display/SD/touch): only Wi-Fi and the network stack.
struct Job {
  String body;
  char sig[65];
  char ssid[33];
  char pass[64];
  const char* url;
};
Job job;
volatile bool jobDone = false;
volatile uint8_t jobPhase = 0;  // 1 connecting, 2 uploading
int jobCode = 0;                // HTTP status, or < 0 (see task)
String jobReply;

const int kWifiNotFound = -100, kWifiFailed = -101, kWifiTimeout = -102;

void toHex(const uint8_t* in, size_t n, char* out) {
  static const char* d = "0123456789abcdef";
  for (size_t i = 0; i < n; i++) {
    out[2 * i] = d[in[i] >> 4];
    out[2 * i + 1] = d[in[i] & 15];
  }
  out[2 * n] = 0;
}

bool fromHex(const char* s, uint8_t* out, size_t n) {
  if (strlen(s) != 2 * n) return false;
  for (size_t i = 0; i < n; i++) {
    char b[3] = {s[2 * i], s[2 * i + 1], 0};
    char* end;
    long v = strtol(b, &end, 16);
    if (*end) return false;
    out[i] = (uint8_t)v;
  }
  return true;
}

void deriveKey() {
  uint8_t h[32];
  mbedtls_sha256(secret_, sizeof(secret_), h, 0);
  toHex(h, 8, key_);
  toHex(secret_, sizeof(secret_), secretHex_);
  snprintf(profile_, sizeof(profile_), "%s/g/?k=%s", NG_SYNC_SITE, key_);
}

// /scout/sync.key: "ngsync 1\n<secret hex>\n<seq>\n<claimed 0|1>\n". Like salt.bin: don't share it.
bool loadFile(uint8_t* secret, uint32_t& seq, bool& claimed) {
  String t = storage::readText(kKeyFile);
  if (!t.startsWith("ngsync 1\n")) return false;
  int a = t.indexOf('\n') + 1, b = t.indexOf('\n', a), c = t.indexOf('\n', b + 1);
  if (b < 0 || c < 0) return false;
  if (!fromHex(t.substring(a, b).c_str(), secret, 32)) return false;
  seq = (uint32_t)strtoul(t.substring(b + 1, c).c_str(), nullptr, 10);
  claimed = t.substring(c + 1).toInt() == 1;
  return true;
}

void save() {
  Preferences p;
  p.begin(kNs, false);
  p.putBytes("secret", secret_, sizeof(secret_));
  p.putUInt("seq", seq_);
  p.putBool("claimed", claimed_);
  p.end();
  char t[128];
  snprintf(t, sizeof(t), "ngsync 1\n%s\n%lu\n%d\n", secretHex_, (unsigned long)seq_, claimed_ ? 1 : 0);
  storage::writeTextAtomic(kKeyFile, t);
}

void sign(const String& body, char* out) {
  uint8_t mac[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), secret_, sizeof(secret_),
                  (const uint8_t*)body.c_str(), body.length(), mac);
  toHex(mac, sizeof(mac), out);
}

String buildBody() {
  const Stats& st = engine.stats();
  const peercodec::Info& me = peer::self();
  JsonDocument d;
  d["v"] = 1;
  d["key"] = key_;
  d["seq"] = seq_;
  d["fw"] = NG_FW_VERSION;
  d["name"] = me.name;
  d["hue"] = me.hue;
  d["hat"] = engine.settings().hat;
  d["xp"] = st.xp;
  d["level"] = engine.level();
  d["hats"] = engine.hatMask();
  if (!claimed_) d["claim"] = secretHex_;  // only until the server has registered us
  JsonObject c = d["c"].to<JsonObject>();
  c["wifi"] = st.wifiUnique;
  c["ssid"] = st.ssidUnique;
  c["wifi5g"] = st.wifi5g;
  c["open"] = st.wifiOpen;
  c["wep"] = st.wifiWep;
  c["wpa3"] = st.wifiWpa3;
  c["ent"] = st.wifiEnterprise;
  c["wifi6"] = st.wifi6;
  c["wps"] = st.wifiWps;
  c["hidden"] = st.wifiHidden;
  c["dfs"] = st.wifiDfs;
  c["channels"] = (uint32_t)st.channels.count();
  c["ble"] = st.bleUnique;
  c["bleSightings"] = st.bleSightings;
  c["ibeacon"] = st.bleIBeacon;
  c["eddystone"] = st.bleEddystone;
  c["mesh"] = st.t154Unique;
  c["pans"] = st.t154Pans;
  c["zigbee"] = st.zigbeePans;
  c["thread"] = st.threadPans;
  c["channels154"] = (uint32_t)st.channels154.count();
  c["goblins"] = st.peersMet;
  c["encounters"] = st.peerEncounters;
  c["sniffs"] = st.sniffOffs;
  c["sniffWins"] = st.sniffWins;
  c["trackers"] = st.trackersSeen;
  c["trackerAlerts"] = st.trackerAlerts;
  c["cells"] = st.geoCells;
  c["bestStreak"] = st.bestStreak;
  c["quests"] = st.questsDone;
  c["boards"] = st.boardsCleared;
  c["pets"] = st.pets;
  c["scans"] = st.scans;
  c["sessions"] = st.sessions;
  c["uptimeMin"] = st.uptimeMin;
  c["batteryMin"] = st.batteryMin;
  c["nightMin"] = st.nightMin;
  JsonArray ach = d["ach"].to<JsonArray>();
  for (size_t i = 0; i < ACHIEVEMENT_COUNT; i++)
    if (st.achieved[i]) ach.add(ACHIEVEMENTS[i].id);
  String out;
  serializeJson(d, out);
  return out;
}

void uploadTask(void*) {
  jobPhase = 1;
  WiFi.persistent(false);  // the credentials live in our own NVS namespace, not the Wi-Fi driver's
  WiFi.begin(job.ssid, job.pass[0] ? job.pass : nullptr);
  uint32_t t0 = millis();
  wl_status_t st = WiFi.status();
  while (st != WL_CONNECTED && millis() - t0 < SYNC_WIFI_TIMEOUT_MS) {
    delay(200);
    st = WiFi.status();
  }
  if (st != WL_CONNECTED) {
    jobCode = st == WL_NO_SSID_AVAIL ? kWifiNotFound : st == WL_CONNECT_FAILED ? kWifiFailed : kWifiTimeout;
  } else {
    jobPhase = 2;
    NetworkClientSecure client;
    client.useBuiltinCACertBundle();  // checks the server's certificate (ESP-IDF's root CA bundle)
    client.setHandshakeTimeout(15);
    HTTPClient http;
    http.setTimeout(15000);
    if (http.begin(client, job.url)) {
      http.addHeader("Content-Type", "application/json");
      http.addHeader("X-Goblin-Sig", job.sig);
      http.setUserAgent("NGScout/" NG_FW_VERSION);
      jobCode = http.POST((uint8_t*)job.body.c_str(), job.body.length());
      if (jobCode > 0) jobReply = http.getString();
      http.end();
    } else {
      jobCode = -1;
    }
  }
  WiFi.disconnect(false, true);
  jobDone = true;
  vTaskDelete(nullptr);
}

void finish(goblinsync::State s) {
  status_.state = s;
  status_.atMs = millis();
  job.body = String();  // free it
  jobReply = String();
}

void handleReply() {
  goblinsync::Status& s = status_;
  s.motd[0] = 0;
  s.msg[0] = 0;
  JsonDocument r;
  bool parsed = jobCode > 0 && !deserializeJson(r, jobReply);
  switch (jobCode) {
    case 200:
      if (!parsed || !(r["ok"] | false)) break;
      if (forgetting_) {  // the server deleted our entry; a later Sync registers us again
        claimed_ = false;
        save();
        strlcpy(s.msg, "Removed from the leaderboard", sizeof(s.msg));
        log_i("sync: removed from the leaderboard");
        finish(goblinsync::State::Done);
        return;
      }
      s.rank = r["rank"] | 0;
      s.of = r["of"] | 0;
      strlcpy(s.motd, r["motd"] | "", sizeof(s.motd));
      snprintf(s.msg, sizeof(s.msg), "Synced! #%lu of %lu", (unsigned long)s.rank, (unsigned long)s.of);
      if (!claimed_) {
        claimed_ = true;
        save();
      }
      engine.synced(s.rank, s.of);
      log_i("sync: ok, rank %lu of %lu", (unsigned long)s.rank, (unsigned long)s.of);
      finish(goblinsync::State::Done);
      return;
    case 409:  // our sequence number went backwards (an older SD card?): jump ahead
      seq_ += 1000;
      save();
      strlcpy(s.msg, "Out of step: press Sync again", sizeof(s.msg));
      break;
    case 429: strlcpy(s.msg, "Too soon: wait a minute", sizeof(s.msg)); break;
    case 401: strlcpy(s.msg, "Server doesn't know this goblin", sizeof(s.msg)); break;
    case 400: strlcpy(s.msg, "Server refused: update firmware?", sizeof(s.msg)); break;
    case kWifiNotFound: strlcpy(s.msg, "Wi-Fi network not found", sizeof(s.msg)); break;
    case kWifiFailed: strlcpy(s.msg, "Wi-Fi refused: wrong password?", sizeof(s.msg)); break;
    case kWifiTimeout: strlcpy(s.msg, "Wi-Fi didn't connect", sizeof(s.msg)); break;
    default:
      if (jobCode >= 500) strlcpy(s.msg, "Server trouble: try later", sizeof(s.msg));
      else if (jobCode < 0) strlcpy(s.msg, "Couldn't reach the server", sizeof(s.msg));
      else snprintf(s.msg, sizeof(s.msg), "Server said %d", jobCode);
  }
  if (jobCode == 200 && !s.msg[0]) strlcpy(s.msg, "Odd reply from server", sizeof(s.msg));
  log_w("sync: failed (%d)", jobCode);
  finish(goblinsync::State::Failed);
}
}  // namespace

namespace goblinsync {

void begin() {
  Preferences p;
  p.begin(kNs, false);
  if (p.isKey("ssid")) p.getString("ssid", ssid_, sizeof(ssid_));
  if (p.isKey("pass")) p.getString("pass", pass_, sizeof(pass_));
  bool nvs = p.isKey("secret") && p.getBytes("secret", secret_, sizeof(secret_)) == sizeof(secret_);
  seq_ = p.isKey("seq") ? p.getUInt("seq", 0) : 0;
  claimed_ = p.isKey("claimed") && p.getBool("claimed", false);
  p.end();

  uint8_t fileSecret[32];
  uint32_t fileSeq = 0;
  bool fileClaimed = false;
  bool file = loadFile(fileSecret, fileSeq, fileClaimed);
  if (!nvs && file) {  // a factory flash wiped NVS: the card remembers who we are
    memcpy(secret_, fileSecret, sizeof(secret_));
    claimed_ = fileClaimed;
    log_i("sync: identity restored from SD");
  } else if (!nvs) {
    esp_fill_random(secret_, sizeof(secret_));
    claimed_ = false;
  }
  if (file && memcmp(fileSecret, secret_, sizeof(secret_)) == 0 && fileSeq > seq_) seq_ = fileSeq;
  deriveKey();
  save();
  log_i("sync: key %s, %s", key_, ssid_[0] ? "Wi-Fi set" : "no Wi-Fi yet");
}

bool hasWifi() { return ssid_[0] != 0; }
const char* ssid() { return ssid_; }
const char* key() { return key_; }
const char* profileUrl() { return profile_; }

void setWifi(const char* ssid, const char* pass) {
  strlcpy(ssid_, ssid ? ssid : "", sizeof(ssid_));
  strlcpy(pass_, pass ? pass : "", sizeof(pass_));
  Preferences p;
  p.begin(kNs, false);
  p.putString("ssid", ssid_);
  p.putString("pass", pass_);
  p.end();
  status_ = Status();
}

void forget() {
  if (busy() || !claimed_) return;
  if (!hasWifi()) {
    strlcpy(status_.msg, "Pick a Wi-Fi network first", sizeof(status_.msg));
    status_.state = State::Failed;
    return;
  }
  status_ = Status();
  status_.state = State::Waiting;
  strlcpy(status_.msg, "Finishing the current scan...", sizeof(status_.msg));
  waitSince_ = millis();
  forgetting_ = true;
}

bool registered() { return claimed_; }

void start() {
  if (busy()) return;
  forgetting_ = false;
  if (!hasWifi()) {
    strlcpy(status_.msg, "Pick a Wi-Fi network first", sizeof(status_.msg));
    status_.state = State::Failed;
    return;
  }
  status_ = Status();
  status_.state = State::Waiting;
  strlcpy(status_.msg, "Finishing the current scan...", sizeof(status_.msg));
  waitSince_ = millis();
}

void tick(ScanManager& scans) {
  switch (status_.state) {
    case State::Waiting:
      scans.pause(true);
      if (!scans.idle() && millis() - waitSince_ < 15000) return;
      seq_++;
      save();  // before sending: a reply we never see must not let this seq be reused
      if (forgetting_) {
        JsonDocument d;
        d["v"] = 1;
        d["key"] = key_;
        d["seq"] = seq_;
        job.body = String();
        serializeJson(d, job.body);
        job.url = NG_FORGET_URL;
      } else {
        job.body = buildBody();
        job.url = NG_SYNC_URL;
      }
      sign(job.body, job.sig);
      strlcpy(job.ssid, ssid_, sizeof(job.ssid));
      strlcpy(job.pass, pass_, sizeof(job.pass));
      jobDone = false;
      jobPhase = 0;
      jobCode = 0;
      if (xTaskCreate(uploadTask, "ngsync", 12288, nullptr, 1, nullptr) != pdPASS) {
        strlcpy(status_.msg, "Out of memory", sizeof(status_.msg));
        finish(State::Failed);
        scans.pause(false);
        return;
      }
      status_.state = State::Connecting;
      snprintf(status_.msg, sizeof(status_.msg), "Joining %s...", ssid_);
      break;
    case State::Connecting:
    case State::Uploading:
      if (jobPhase == 2 && status_.state == State::Connecting) {
        status_.state = State::Uploading;
        strlcpy(status_.msg, forgetting_ ? "Asking the server to forget us..." : "Uploading the hoard...",
                sizeof(status_.msg));
      }
      if (!jobDone) return;
      handleReply();
      scans.pause(false);
      break;
    default:
      break;
  }
}

bool busy() {
  return status_.state == State::Waiting || status_.state == State::Connecting || status_.state == State::Uploading;
}

const Status& status() { return status_; }

}  // namespace goblinsync
