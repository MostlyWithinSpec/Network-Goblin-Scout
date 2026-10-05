// Host unit test for the 802.15.4 header parser. Build & run:
//   g++ -std=c++17 -Wall -Wextra -I src test/test_ieee802154.cpp -o /tmp/t154 && /tmp/t154
#include <cstdio>
#include <vector>
#include "scanners/Ieee802154Frame.h"

using namespace ieee802154;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static Frame run(const std::vector<uint8_t>& b, bool expectOk = true) {
  Frame f;
  bool ok = parse(b.data(), b.size(), f);
  CHECK(ok == expectOk);
  return f;
}

int main() {
  {  // Zigbee data: FCF 0x8841 (data, PAN compress, dst short, src short, v2003), NWK FCF 0x0008
    Frame f = run({0x41, 0x88, 0x0e, 0x22, 0x1a, 0xff, 0xff, 0x00, 0x00, 0x08, 0x00, 0xfc, 0xff, 0x00, 0x00, 0x1e});
    CHECK(f.type == kData); CHECK(f.pan() == 0x1a22); CHECK(f.srcLen == 2); CHECK(f.proto == kZigbee);
  }
  {  // Thread MLE: FCF 0xD841 (data, compress, dst short, src ext, v2006), 6LoWPAN IPHC 0x7f
    Frame f = run({0x41, 0xd8, 0x01, 0xce, 0xfa, 0xff, 0xff, 1, 2, 3, 4, 5, 6, 7, 8, 0x7f, 0x33, 0xf0, 0x4d});
    CHECK(f.pan() == 0xface); CHECK(f.srcLen == 8); CHECK(f.src[7] == 8); CHECK(f.proto == kThread);
  }
  {  // Secured data (Thread): FCF 0xD869
    Frame f = run({0x69, 0xd8, 0x05, 0x34, 0x12, 0x00, 0x00, 9, 9, 9, 9, 9, 9, 9, 9, 0x0d, 0, 0, 0, 0});
    CHECK(f.secured); CHECK(f.pan() == 0x1234); CHECK(f.proto == kThread);
  }
  {  // Zigbee beacon: FCF 0x8000, src PAN + short, superframe, GTS 0, pending 0, protocol id 0
    Frame f = run({0x00, 0x80, 0x10, 0x99, 0x55, 0x00, 0x00, 0xff, 0xcf, 0x00, 0x00, 0x00, 0x22, 0x84});
    CHECK(f.type == kBeacon); CHECK(f.pan() == 0x5599); CHECK(f.proto == kZigbee);
  }
  {  // Thread beacon: FCF 0xC000, ext src, protocol id 3
    Frame f = run({0x00, 0xc0, 0x11, 0xaa, 0xbb, 1, 2, 3, 4, 5, 6, 7, 8, 0xff, 0x0f, 0x00, 0x00, 0x03, 0x91});
    CHECK(f.pan() == 0xbbaa); CHECK(f.srcLen == 8); CHECK(f.proto == kThread);
  }
  {  // Beacon with GTS and pending addresses before the payload
    Frame f = run({0x00, 0x80, 0x10, 0x01, 0x00, 0x02, 0x00, 0xff, 0xcf,
                   0x81, 0x00, 1, 2, 3,      // GTS: 1 descriptor + direction byte
                   0x01, 0x34, 0x12,         // pending: 1 short address
                   0x00, 0x22});
    CHECK(f.proto == kZigbee);
  }
  {  // ACK: no addresses
    Frame f = run({0x02, 0x00, 0x2a});
    CHECK(f.type == kAck); CHECK(f.srcLen == 0);
  }
  {  // 2015 frame, both short, PAN compress: only destination PAN present
    Frame f = run({0x41, 0xa8, 0x07, 0x78, 0x56, 0xff, 0xff, 0x01, 0x00, 0x7f, 0x00});
    CHECK(f.version == 2); CHECK(f.pan() == 0x5678); CHECK(f.srcLen == 2); CHECK(f.proto == kThread);
  }
  {  // 2015 frame, seq number suppressed
    Frame f = run({0x41, 0xa9, 0x78, 0x56, 0xff, 0xff, 0x01, 0x00, 0x7f, 0x00});
    CHECK(f.pan() == 0x5678); CHECK(f.src[0] == 0x01);
  }
  run({0x41, 0xd8, 0x01, 0xce}, false);   // truncated
  run({0x41, 0x84, 0x01, 0, 0, 0, 0}, false);  // reserved dst mode 1
  printf(failures ? "%d FAILED\n" : "all 802.15.4 parser tests passed\n", failures);
  return failures != 0;
}
