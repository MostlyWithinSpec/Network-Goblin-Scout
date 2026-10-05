#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Minimal IEEE 802.15.4 MAC header parser (2003/2006/2015 frame versions).
// Pure C++ with no Arduino dependencies so it can be unit-tested on a PC
// (see test/test_ieee802154.cpp).
namespace ieee802154 {

enum FrameType : uint8_t { kBeacon = 0, kData = 1, kAck = 2, kCommand = 3 };
enum Proto : uint8_t { kUnknown = 0, kZigbee = 1, kThread = 2 };

struct Frame {
  uint8_t type = 0;
  uint8_t version = 0;
  bool secured = false;
  uint16_t dstPan = 0xFFFF, srcPan = 0xFFFF;
  uint8_t srcMode = 0;   // 0 none, 2 short, 3 extended
  uint8_t src[8] = {0};  // little-endian as on air
  uint8_t srcLen = 0;
  size_t payloadOff = 0; // offset of MAC payload (only meaningful if !secured and no IEs)
  bool hasIEs = false;
  Proto proto = kUnknown;

  // The PAN this frame belongs to (source PAN if present, else destination PAN).
  uint16_t pan() const { return srcPan != 0xFFFF ? srcPan : dstPan; }
};

inline uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

inline size_t addrLen(uint8_t mode) { return mode == 2 ? 2 : mode == 3 ? 8 : 0; }

// Which PAN id fields are present. 2003/2006: one rule; 2015: IEEE 802.15.4-2015 table 7-2.
inline void panPresence(uint8_t version, uint8_t dstMode, uint8_t srcMode, bool compress, bool& dstPan,
                        bool& srcPan) {
  if (version < 2) {
    dstPan = dstMode != 0;
    srcPan = srcMode != 0 && !(compress && dstMode != 0);
    return;
  }
  dstPan = srcPan = false;
  if (!dstMode && !srcMode) { dstPan = compress; return; }
  if (dstMode && !srcMode) { dstPan = !compress; return; }
  if (!dstMode && srcMode) { srcPan = !compress; return; }
  if (dstMode == 3 && srcMode == 3) { dstPan = !compress; return; }
  dstPan = true;           // at least one short address
  srcPan = !compress;
}

// Guess the network stack from the MAC payload of an unsecured frame.
inline Proto classify(const Frame& f, const uint8_t* d, size_t len) {
  if (f.secured) return f.type == kData ? kThread : kUnknown;  // Thread uses MAC security; Zigbee secures at NWK
  if (f.hasIEs || f.payloadOff >= len) return kUnknown;
  const uint8_t* p = d + f.payloadOff;
  size_t n = len - f.payloadOff;
  if (f.type == kBeacon) {
    // superframe spec (2), GTS spec (+ list), pending address spec (+ list), then beacon payload
    if (n < 4) return kUnknown;
    size_t o = 2;
    uint8_t gts = p[o++] & 0x07;
    if (gts) o += 1 + 3 * gts;
    if (o >= n) return kUnknown;
    uint8_t pend = p[o++];
    o += 2 * (pend & 0x07) + 8 * ((pend >> 4) & 0x07);
    if (o >= n) return kUnknown;
    if (p[o] == 0x00) return kZigbee;
    if (p[o] == 0x03) return kThread;
    return kUnknown;
  }
  if (f.type == kData && n >= 2) {
    uint8_t b = p[0];
    if ((b & 0xE0) == 0x60 || (b & 0xC0) == 0x80 || b == 0x41) return kThread;  // 6LoWPAN IPHC / mesh / IPv6
    uint8_t nwkType = b & 0x03, nwkVer = (b >> 2) & 0x0F;
    if (nwkType <= 1 && (nwkVer == 2 || nwkVer == 3)) return kZigbee;            // Zigbee NWK header
  }
  return kUnknown;
}

// d/len: the PSDU without the 2-byte FCS. Returns false for frames we can't use.
inline bool parse(const uint8_t* d, size_t len, Frame& f) {
  if (len < 3) return false;
  uint16_t fcf = rd16(d);
  f.type = fcf & 0x07;
  f.secured = fcf & 0x08;
  bool compress = fcf & 0x40;
  bool seqSuppressed = (fcf >> 8) & 0x01;
  f.hasIEs = (fcf >> 9) & 0x01;
  uint8_t dstMode = (fcf >> 10) & 0x03;
  f.version = (fcf >> 12) & 0x03;
  f.srcMode = (fcf >> 14) & 0x03;
  if (dstMode == 1 || f.srcMode == 1) return false;  // reserved modes
  if (f.version < 2) seqSuppressed = false;

  bool dstPanPresent, srcPanPresent;
  panPresence(f.version, dstMode, f.srcMode, compress, dstPanPresent, srcPanPresent);

  size_t o = 2 + (seqSuppressed ? 0 : 1);
  size_t need = o + (dstPanPresent ? 2 : 0) + addrLen(dstMode) + (srcPanPresent ? 2 : 0) + addrLen(f.srcMode);
  if (need > len) return false;

  if (dstPanPresent) { f.dstPan = rd16(d + o); o += 2; }
  o += addrLen(dstMode);
  if (srcPanPresent) { f.srcPan = rd16(d + o); o += 2; }
  else if (f.version < 2 && compress && f.srcMode) f.srcPan = f.dstPan;  // compressed: same PAN
  f.srcLen = (uint8_t)addrLen(f.srcMode);
  memcpy(f.src, d + o, f.srcLen);
  o += f.srcLen;
  f.payloadOff = o;
  f.proto = classify(f, d, len);
  return true;
}

}  // namespace ieee802154
