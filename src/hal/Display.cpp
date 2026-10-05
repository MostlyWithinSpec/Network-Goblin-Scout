#include "Display.h"
#include <SPI.h>
#include <esp_heap_caps.h>
#include "board.h"
#include "config.h"

// Everything draws into a framebuffer in PSRAM; flush() sends it to the ST7789.
//
// Speed notes (ESP32-C5): the Arduino core clocks SPI from the crystal (40 or 48 MHz),
// so the fastest possible pixel clock is the crystal frequency itself; asking for more
// just gets that. flush() also keeps a copy of the last frame and only sends the parts
// of the screen that changed, in 8-row bands.
namespace {
Arduino_DataBus* bus = nullptr;
Arduino_TFT* panel = nullptr;
Arduino_Canvas* canvas = nullptr;
uint16_t* prev = nullptr;        // what the panel currently shows
bool fullRefresh = true;
uint8_t brightness = 80;
bool sleeping = false;
bool fast = true;
uint8_t rowBuf[SCREEN_W * 2];    // internal RAM: one byte-swapped row

const int16_t kBand = 8;

void applyBacklight() {
  uint32_t duty = sleeping ? 0 : (uint32_t)brightness * 255 / 100;
  ledcWrite(PIN_TFT_BL, duty);
}

uint32_t requestedHz() { return fast ? DISPLAY_SPI_HZ_FAST : DISPLAY_SPI_HZ; }

// Columns [x0, x1] where row `y` differs from the previous frame; false if identical.
bool rowDiff(const uint16_t* fb, int16_t y, int16_t& x0, int16_t& x1) {
  const uint32_t* a = (const uint32_t*)(fb + y * SCREEN_W);
  const uint32_t* b = (const uint32_t*)(prev + y * SCREEN_W);
  const int16_t n = SCREEN_W / 2;
  int16_t i = 0;
  while (i < n && a[i] == b[i]) i++;
  if (i == n) return false;
  int16_t j = n - 1;
  while (j > i && a[j] == b[j]) j--;
  x0 = i * 2;
  x1 = j * 2 + 1;
  return true;
}

void sendRow(const uint16_t* src, int16_t w) {
  for (int16_t i = 0; i < w; i++) {  // the panel wants big-endian pixels
    rowBuf[2 * i] = src[i] >> 8;
    rowBuf[2 * i + 1] = src[i] & 0xFF;
  }
  bus->writeBytes(rowBuf, w * 2);
}
}  // namespace

namespace display {

bool begin() {
  // Shared bus: the SPI object is started once in main; HWSPI reuses it.
  bus = new Arduino_HWSPI(PIN_TFT_DC, PIN_TFT_CS, PIN_SPI_SCK, PIN_SPI_MOSI,
                          PIN_SPI_MISO, &SPI, true /* shared */);
  panel = new Arduino_ST7789(bus, PIN_TFT_RST, DISPLAY_ROTATION, false /* IPS */, 240, 320);
  canvas = new Arduino_Canvas(SCREEN_W, SCREEN_H, panel);
  if (!canvas->begin(requestedHz())) {
    log_e("display: canvas begin failed (framebuffer alloc?)");
    return false;
  }
  prev = (uint16_t*)heap_caps_malloc(SCREEN_W * SCREEN_H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!prev) log_w("display: no memory for partial updates, sending full frames");
  log_i("display: crystal %lu MHz, SPI asked %lu MHz (the core caps it at the crystal)",
        (unsigned long)getXtalFrequencyMhz(), (unsigned long)(requestedHz() / 1000000));
  ledcAttach(PIN_TFT_BL, 5000, 8);
  applyBacklight();
  canvas->fillScreen(0x0000);
  flush();
  return true;
}

Arduino_Canvas* gfx() { return canvas; }

uint16_t* framebuffer() { return canvas ? canvas->getFramebuffer() : nullptr; }

void flush() {
  if (!canvas || sleeping) return;
  uint16_t* fb = canvas->getFramebuffer();
  if (!fb) return;
  panel->startWrite();
  for (int16_t y0 = 0; y0 < SCREEN_H; y0 += kBand) {
    int16_t h = SCREEN_H - y0 < kBand ? SCREEN_H - y0 : kBand;
    int16_t x0 = SCREEN_W, x1 = -1;
    if (!prev || fullRefresh) {
      x0 = 0;
      x1 = SCREEN_W - 1;
    } else {
      for (int16_t y = y0; y < y0 + h; y++) {
        int16_t a, b;
        if (rowDiff(fb, y, a, b)) {
          if (a < x0) x0 = a;
          if (b > x1) x1 = b;
        }
      }
    }
    if (x1 < x0) continue;  // band unchanged: send nothing
    int16_t w = x1 - x0 + 1;
    panel->writeAddrWindow(x0, y0, w, h);
    for (int16_t y = y0; y < y0 + h; y++) {
      const uint16_t* src = fb + y * SCREEN_W + x0;
      sendRow(src, w);
      if (prev) memcpy(prev + y * SCREEN_W + x0, src, w * 2);
    }
  }
  panel->endWrite();
  fullRefresh = false;
}

void setFast(bool on) {
  fast = on;
  if (bus) bus->begin(requestedHz());
  fullRefresh = true;
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
  if (!on) fullRefresh = true;
  applyBacklight();
}

bool asleep() { return sleeping; }

}  // namespace display
