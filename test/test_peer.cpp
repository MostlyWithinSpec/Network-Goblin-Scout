// Host unit test for the goblin beacon codec. Build & run:
//   g++ -std=c++17 -Wall -Wextra -I src test/test_peer.cpp -o /tmp/tpeer && /tmp/tpeer
#include <cstdio>
#include "social/PeerCodec.h"

using namespace peercodec;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main() {
  Info a;
  a.id = 0xDEADBEEF;
  a.level = 513;
  a.hue = 200;
  a.flags = 1;
  nameFor(a.id, a.name, sizeof(a.name));
  CHECK(strlen(a.name) > 3 && strlen(a.name) <= kMaxName);

  uint8_t buf[kMaxLen];
  size_t n = encode(a, buf);
  CHECK(n <= kMaxLen);
  CHECK(n + 2 /*manuf AD header*/ + 3 /*flags AD*/ <= 31);

  Info b;
  CHECK(decode(buf, n, b));
  CHECK(b.id == a.id); CHECK(b.level == 513); CHECK(b.hue == 200); CHECK(b.flags == 1);
  CHECK(strcmp(b.name, a.name) == 0);

  // longest possible name still fits
  for (uint32_t id = 0; id < 256; id++) {
    char nm[kMaxName + 1];
    nameFor(id, nm, sizeof(nm));
    CHECK(strlen(nm) <= kMaxName);
  }

  buf[2] = 'X';
  CHECK(!decode(buf, n, b));                       // wrong magic
  CHECK(!decode(buf, 5, b));                       // too short
  uint8_t apple[] = {0x4C, 0x00, 0x02, 0x15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  CHECK(!decode(apple, sizeof(apple), b));         // someone else's manufacturer data

  printf(failures ? "%d FAILED\n" : "all peer codec tests passed\n", failures);
  return failures != 0;
}
