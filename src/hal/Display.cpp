#include "Display.h"
#include <SPI.h>
#include "board.h"
#include "config.h"

namespace {
Arduino_DataBus* bus = nullptr;
Arduino_GFX* panel = nullptr;
Arduino_Canvas* canvas = nullptr;
uint8_t brightness = 80;
bool sleeping = false;

void applyBacklight() {
  uint32_t duty = sleeping ? 0 : (uint32_t)brightness * 255 / 100;
  ledcWrite(PIN_TFT_BL, duty);
}
}  // namespace

namespace display {

bool begin() {
  // Shared bus: the SPI object is started once in main; HWSPI reuses it.
  bus = new Arduino_HWSPI(PIN_TFT_DC, PIN_TFT_CS, PIN_SPI_SCK, PIN_SPI_MOSI,
                          PIN_SPI_MISO, &SPI, true /* shared */);
  panel = new Arduino_ST7789(bus, PIN_TFT_RST, DISPLAY_ROTATION, false /* IPS */, 240, 320);
  canvas = new Arduino_Canvas(SCREEN_W, SCREEN_H, panel);
  if (!canvas->begin(DISPLAY_SPI_HZ)) {
    log_e("display: canvas begin failed (framebuffer alloc?)");
    return false;
  }
  ledcAttach(PIN_TFT_BL, 5000, 8);
  applyBacklight();
  canvas->fillScreen(0x0000);
  canvas->flush();
  return true;
}

Arduino_Canvas* gfx() { return canvas; }

uint16_t* framebuffer() { return canvas ? canvas->getFramebuffer() : nullptr; }

void flush() {
  if (canvas && !sleeping) canvas->flush();
}

void setBrightness(uint8_t pct) {
  brightness = pct > 100 ? 100 : (pct < 5 ? 5 : pct);
  applyBacklight();
}

void setInverted(bool inv) {
  if (panel) panel->invertDisplay(inv);
}

void sleep(bool on) {
  sleeping = on;
  applyBacklight();
}

bool asleep() { return sleeping; }

}  // namespace display
