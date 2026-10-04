#include "Sprites.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include "../hal/Storage.h"
#include "config.h"

namespace {
// Keys match metadata.json "states". Some have aliases for convenience.
const char* kKeys[] = {"idle",   "scanning",    "searching", "discovered", "excited",  "level_up",
                       "achievement", "uploading", "sleep",     "low_battery", "offline", "sync_complete"};
const char* kAlias[] = {nullptr, "scan", nullptr, "happy", nullptr, "levelup",
                        nullptr, nullptr, "sleeping", nullptr, nullptr, "synced"};

uint16_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }
uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

bool loadBmp(const String& path, Frame& out) {
  File f = SD.open(path, FILE_READ);
  if (!f) { log_w("sprite: missing %s", path.c_str()); return false; }
  uint8_t h[70];
  if (f.read(h, sizeof(h)) < 54 || h[0] != 'B' || h[1] != 'M') { f.close(); return false; }
  uint32_t dataOff = rd32(h + 10);
  int32_t w = (int32_t)rd32(h + 18);
  int32_t ht = (int32_t)rd32(h + 22);
  uint16_t bpp = rd16(h + 28);
  uint32_t comp = rd32(h + 30);
  bool bottomUp = ht > 0;
  if (ht < 0) ht = -ht;
  if (w <= 0 || ht <= 0 || w > SPRITE_BOX || ht > SPRITE_BOX) {
    log_w("sprite: %s is %dx%d (max %d)", path.c_str(), w, ht, SPRITE_BOX);
    f.close(); return false;
  }
  bool is555 = false;
  if (bpp == 16) is555 = (comp == 0) || (comp == 3 && rd32(h + 54) == 0x7C00);
  else if (bpp != 24 && bpp != 32) { log_w("sprite: %s unsupported %u bpp", path.c_str(), bpp); f.close(); return false; }

  size_t bytesPx = bpp / 8;
  size_t rowBytes = ((w * bytesPx) + 3) & ~3u;
  uint16_t* px = (uint16_t*)heap_caps_malloc(w * ht * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!px) px = (uint16_t*)malloc(w * ht * 2);
  std::vector<uint8_t> row(rowBytes);
  if (!px) { f.close(); return false; }

  for (int32_t y = 0; y < ht; y++) {
    int32_t srcRow = bottomUp ? (ht - 1 - y) : y;
    f.seek(dataOff + srcRow * rowBytes);
    f.read(row.data(), rowBytes);
    for (int32_t x = 0; x < w; x++) {
      const uint8_t* p = &row[x * bytesPx];
      uint16_t c;
      if (bpp == 16) {
        uint16_t v = rd16(p);
        c = is555 ? (uint16_t)(((v & 0x7C00) << 1) | ((v & 0x03E0) << 1) | (v & 0x001F)) : v;
      } else {  // BGR(A)
        c = ((p[2] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[0] >> 3);
        if (bpp == 32 && p[3] < 128) c = SPRITE_TRANSPARENT;
      }
      px[y * w + x] = c;
    }
  }
  f.close();
  out.px = px; out.w = w; out.h = ht;
  return true;
}
}  // namespace

const char* cstateKey(CState s) { return kKeys[(size_t)s]; }

void SpritePack::clear() {
  for (auto& v : frames_) {
    for (auto& fr : v) if (fr.px) heap_caps_free(fr.px);
    v.clear();
  }
  loaded_ = false;
}

bool SpritePack::load(const String& name) {
  clear();
  name_ = name;
  author_ = "";
  String dir = String(NG_SPRITE_DIR) + "/" + name;
  String meta = storage::readText((dir + "/metadata.json").c_str());
  if (meta.isEmpty()) return false;
  JsonDocument doc;
  if (deserializeJson(doc, meta)) { log_w("sprite: bad metadata in %s", dir.c_str()); return false; }
  author_ = doc["author"] | "";
  JsonObject states = doc["states"];
  size_t total = 0;
  for (size_t i = 0; i < (size_t)CState::COUNT; i++) {
    JsonArray files = states[kKeys[i]];
    if (files.isNull() && kAlias[i]) files = states[kAlias[i]];
    if (files.isNull()) continue;
    for (JsonVariant v : files) {
      Frame fr;
      if (loadBmp(dir + "/" + v.as<const char*>(), fr)) { frames_[i].push_back(fr); total++; }
    }
  }
  loaded_ = total > 0;
  log_i("sprite: pack '%s' loaded %u frames", name.c_str(), total);
  return loaded_;
}

const Frame* SpritePack::frame(CState s, uint32_t tick) const {
  if (!loaded_) return nullptr;
  const auto* v = &frames_[(size_t)s];
  if (v->empty()) v = &frames_[(size_t)CState::Idle];
  if (v->empty()) return nullptr;
  return &(*v)[tick % v->size()];
}

std::vector<String> listSpritePacks() {
  std::vector<String> out;
  if (!storage::ok()) return out;
  File root = SD.open(NG_SPRITE_DIR);
  if (!root) return out;
  for (File e = root.openNextFile(); e; e = root.openNextFile()) {
    if (e.isDirectory()) out.push_back(String(e.name()));
    e.close();
  }
  root.close();
  return out;
}
