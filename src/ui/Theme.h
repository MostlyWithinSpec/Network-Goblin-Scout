#pragma once
#include "assets/Fonts.h"
#include "gfx/Surface.h"

// Colours and layout shared by every screen. Palette is pulled from the logo:
// goblin green, the cyan glow of its device, and the red of its eyes.
namespace theme {
using gfx::hex;

const uint16_t kBgTop = hex(0x071A22);
const uint16_t kBgBottom = hex(0x020709);
const uint16_t kPanel = hex(0x0C1D24);
const uint16_t kPanelHi = hex(0x14303A);
const uint16_t kEdge = hex(0x1F4E58);
const uint16_t kText = hex(0xE6F6F2);
const uint16_t kDim = hex(0x7E9EA0);
const uint16_t kFaint = hex(0x3A5A60);

const uint16_t kCyan = hex(0x3BE8DA);    // device glow, primary accent
const uint16_t kGreen = hex(0x8EE34F);   // goblin skin
const uint16_t kRed = hex(0xFF4A3D);     // eyes
const uint16_t kAmber = hex(0xFFB627);
const uint16_t kMagenta = hex(0xFF4FD8); // other goblins
const uint16_t kViolet = hex(0x9B6BFF);  // 802.15.4 mesh
const uint16_t kBlue = hex(0x4FA3FF);    // Bluetooth

const uint16_t kBronzeC = hex(0xD08A4A);
const uint16_t kSilverC = hex(0xC9D6E0);
const uint16_t kGoldC = hex(0xFFD54A);

const int16_t W = 320, H = 240;
const int16_t kStatusH = 20;
const int16_t kNavH = 36;
const int16_t kBodyY = kStatusH;
const int16_t kBodyH = H - kStatusH - kNavH;

inline const gfx::Font& fSmall() { return fonts::kSmall; }
inline const gfx::Font& fBody() { return fonts::kBody; }
inline const gfx::Font& fTitle() { return fonts::kTitle; }
inline const gfx::Font& fBig() { return fonts::kBig; }
inline const gfx::Font& fHuge() { return fonts::kHuge; }
}  // namespace theme
