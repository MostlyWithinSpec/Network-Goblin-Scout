#pragma once
#include <stdint.h>
#include "gfx/Surface.h"

// The goblin's pet: a little cat drawn in vector shapes (like the hats), so it animates at any
// size. A cat is a coat recipe; Pip and Lily (the owner's cats) are the built-in ones.
namespace ui {

struct CatCoat {
  const char* name;
  uint16_t base;       // main fur
  uint16_t patch;      // tortie / calico patches
  uint16_t patch2;     // second patch shade (brindle mottling)
  uint16_t eyes;
  float size;          // 1 = a grown cat, smaller = kitten
  bool bib;            // white chest and chin
  bool whitePaws;      // all four paws white
  bool whiteTip;       // one white-tipped front paw
  bool halfFace;       // a ginger patch over one side of the face
  bool blaze;          // a ginger stripe down the forehead and nose
  uint8_t brindle;     // how many mottled specks over the coat (tortie)
  bool fluffy;         // kitten fluff on the cheeks
};

extern const CatCoat kPip;   // torico: black, white bib and paws, ginger patches, a ginger half-face
extern const CatCoat kLily;  // tiny tortie: brindled black and ginger, ginger blaze, big eyes

enum class PetPose : uint8_t { Sit, Alert, Hop, Sleep };

// (x, y) = between the front paws, on the ground. k = scale (1 = about 46 px tall sitting).
// `seed` makes each cat's blinks and tail out of step with the other's.
void drawCat(gfx::Surface& s, const CatCoat& c, float x, float y, float k, uint32_t now, PetPose pose,
             uint32_t seed);

}  // namespace ui
