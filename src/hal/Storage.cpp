#include "Storage.h"
#include <SPI.h>
#include "board.h"
#include "config.h"

namespace {
bool mounted = false;
}

namespace storage {

bool begin() {
  mounted = SD.begin(PIN_SD_CS, SPI, 20000000);
  if (!mounted) {
    log_w("storage: no microSD card (running without persistence)");
    return false;
  }
  ensureDir(NG_DATA_DIR);
  ensureDir(NG_SPRITE_DIR);
  log_i("storage: SD mounted, %llu MB", SD.cardSize() / (1024ULL * 1024ULL));
  return true;
}

bool ok() { return mounted; }

bool ensureDir(const char* path) {
  if (!mounted) return false;
  if (SD.exists(path)) return true;
  return SD.mkdir(path);
}

String readText(const char* path) {
  if (!mounted) return String();
  File f = SD.open(path, FILE_READ);
  if (!f) return String();
  String s = f.readString();
  f.close();
  return s;
}

bool writeTextAtomic(const char* path, const String& text) {
  if (!mounted) return false;
  String tmp = String(path) + ".tmp";
  File f = SD.open(tmp, FILE_WRITE);
  if (!f) return false;
  size_t n = f.print(text);
  f.close();
  if (n != text.length()) return false;
  if (SD.exists(path)) SD.remove(path);
  return SD.rename(tmp, path);
}

bool append(const char* path, const String& text) {
  if (!mounted || text.isEmpty()) return false;
  File f = SD.open(path, FILE_APPEND);
  if (!f) return false;
  size_t n = f.print(text);
  f.close();
  return n == text.length();
}

bool readBytes(const char* path, uint8_t* buf, size_t len) {
  if (!mounted) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  size_t n = f.read(buf, len);
  f.close();
  return n == len;
}

bool writeBytes(const char* path, const uint8_t* buf, size_t len) {
  if (!mounted) return false;
  File f = SD.open(path, FILE_WRITE);
  if (!f) return false;
  size_t n = f.write(buf, len);
  f.close();
  return n == len;
}

}  // namespace storage
