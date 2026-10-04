#include "SeenStore.h"
#include "../hal/Storage.h"

namespace {
int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}  // namespace

size_t SeenStore::load() {
  if (!storage::ok()) return 0;
  File f = SD.open(path_, FILE_READ);
  if (!f) return 0;

  // Chunked parse: take the leading hex token of each line, skip the rest.
  uint8_t buf[1024];
  uint64_t acc = 0;
  int digits = 0;
  bool inId = true;
  size_t loaded = 0;
  while (true) {
    int n = f.read(buf, sizeof(buf));
    if (n <= 0) break;
    for (int i = 0; i < n; i++) {
      char c = (char)buf[i];
      if (c == '\n') {
        if (digits > 0 && acc) { set_.insert(acc); loaded++; }
        acc = 0; digits = 0; inId = true;
        continue;
      }
      if (!inId) continue;
      int v = hexVal(c);
      if (v < 0 || digits >= 16) { inId = false; continue; }
      acc = (acc << 4) | (uint64_t)v;
      digits++;
    }
  }
  if (digits > 0 && acc) { set_.insert(acc); loaded++; }
  f.close();
  return loaded;
}

bool SeenStore::add(uint64_t id, const String& extra) {
  if (!set_.insert(id)) return false;
  char hex[17];
  snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)id);
  pending_ += hex;
  if (extra.length()) { pending_ += ','; pending_ += extra; }
  pending_ += '\n';
  if (pending_.length() > 4096) flush();
  return true;
}

void SeenStore::flush() {
  if (pending_.isEmpty()) return;
  if (storage::append(path_, pending_) || !storage::ok()) pending_ = String();
}
