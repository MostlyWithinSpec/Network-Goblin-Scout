# CLAUDE.md — Network Goblin Scout (NG Scout)

Passive wireless-discovery "digital pet" firmware for the **NM-CYD-C5** board
(ESP32-C5-WROOM-1-N16R8: 16 MB flash, 8 MB PSRAM, dual-band Wi-Fi 6, BLE 5, 802.15.4).
See README.md for the user-facing overview.

The owner has the physical board; Claude does not. **Never claim something works on
hardware — only that it compiles.** Hardware results come from the owner's flash reports.
The owner is an IT person, not an embedded engineer: explain hardware-specific decisions
in a sentence or two. Prefer small, focused commits.

## Build

```sh
pip install platformio==6.2.0   # same version CI uses
pio run                          # build -> .pio/build/ng-scout/firmware.bin + firmware.factory.bin
pio run -t upload                # flash over USB
pio device monitor               # serial log, 115200
```

- Platform: pioarduino `55.03.312` (Arduino core 3.3.12, ESP-IDF 5.5), pinned in
  `platformio.ini`. The stock PlatformIO espressif32 platform has no ESP32-C5 support.
- Board definition: `esp32-c5-devkitc1-n16r4` (same 16 MB flash; PSRAM size is detected at boot).
- `firmware.factory.bin` = bootloader + partitions + app, flash at `0x0`.
  `firmware.bin` = app only, flash at `0x10000`.
- Our code (`src/`) builds with `-Wall -Wextra`; **zero warnings is enforced by CI**.
- Logging: framework/libraries at level 2 (warn) because the BLE library logs every
  advertiser's raw MAC at info; our code is forced to level 3 (info) by
  `include/ng_log_level.h`. Use `log_i/log_w/log_e`.
- CI: `.github/workflows/build.yml` builds every push and uploads the binaries as an artifact.

### Building in a sandbox without the PlatformIO registry

If `api.registry.platformio.org` / `dl.registry.platformio.org` are blocked, `pio run`
fails installing `tool-scons` and the three `lib_deps`. Tell the owner which hosts
are blocked rather than silently working around it. pioarduino also overrides
`REQUESTS_CA_BUNDLE`/`SSL_CERT_FILE` with its own certifi bundle, so behind a TLS-
intercepting proxy you must append the proxy CA to
`~/.platformio/penv/lib/python3.*/site-packages/certifi/cacert.pem`.

## Pin map (from SCH_NM-CYD-C5-v1.0.pdf — trust this over guesses)

| Function | GPIO | Notes |
|---|---|---|
| SPI SCK / MOSI / MISO | 6 / 7 / 2 | Shared by display, touch and SD |
| ST7789 240x320 CS / DC | 23 / 24 | RST is tied to the chip reset |
| Backlight | 25 | PWM via AO3400A |
| XPT2046 touch CS / IRQ | 1 / 3 | IRQ has a 10k pull-up; vendor config omits it |
| microSD CS | 10 | |
| WS2812 RGB LED | 27 | Strapping pin |
| Speaker | 26 | -> SC8002B amp -> 2-pin VO+/VO- header (a speaker output, NOT a battery input) |
| BOOT button | 28 | Active low; boot-mode strapping pin |
| GPS UART RX / TX | 4 / 5 | P5 header; P5 pin 1 = 5 V via P-FET (future battery input) |
| I2C SDA / SCL | 9 / 8 | CN1, shared with on-board AHT20 (0x38). Vendor Bruce config lists them swapped |
| UART0 TX / RX | 11 / 12 | Goes to the CH340 USB-C port |

No battery charger, no battery sense, no RTC. All ADC-capable pins are in use.
Pins live in `include/board.h`; tunables in `include/config.h`.

## Design rules (non-negotiable)

1. **Never store raw MACs or SSIDs** — only salted SHA-256 ids (`Engine::id`). Don't log
   them either. GPS coordinates stay on the SD card (not on screen, serial, or in sync).
2. **BLE rotating private addresses count as sightings, not unique devices.**
3. **Works fully offline; SD card optional** — everything must degrade gracefully without it.
4. **Scanner architecture**: every radio implements `Scanner` (`src/scanners/Scanner.h`)
   and is registered with `ScanManager`. The Engine doesn't care where a `Sighting` came from.
5. **BLE callbacks never touch SPI** (display, SD and touch share one bus) — queue only;
   the main loop drains the queue.
6. Achievement ids in `ACHIEVEMENTS[]` are saved to SD: append only, never reorder or rename.

## Layout

```
include/board.h, config.h, ng_log_level.h
src/main.cpp         setup + non-blocking loop
src/hal/             Display (PSRAM canvas), Touch (XPT2046), Storage (SD), Gps, Fx (LED/speaker), Aht20
src/scanners/        Scanner interface, WifiScanner, BleScanner, ThreadScanner (stub), ScanManager
src/core/            Engine (sighting -> XP -> achievements -> persistence), Achievements, SeenStore, HashSet64
src/ui/              Ui screens, Companion (pet), Sprites (BMP packs from SD)
src/diag/            Hardware bring-up mode
```

## Hardware bring-up mode

Power on, then **press BOOT within 1.5 s** (while the splash says "press BOOT now").
Holding BOOT *during* power-on may enter the ESP32-C5's USB flashing mode instead.
The screen shows, and serial prints every 2 s (lines prefixed `[diag]`): chip/flash/PSRAM,
framebuffer location, SD write test, raw touch, Wi-Fi/BLE scan counts, GPS NMEA counters,
AHT20 reading (tries both I2C pin orders), I2C scan, BOOT state, LED colour cycle,
colour swatches and corner marks. Press RESET to exit. Counts only — no MACs/SSIDs/coordinates.

## Known hardware unknowns (resolve from flash reports, then update this list)

- **Touch axis mapping/calibration**: `TOUCH_*` in `config.h` are guesses (swap XY, no
  inversion, X 185–3700, Y 250–3800). The vendor's TFT_eSPI calibration is
  `{225, 3413, 403, 3334, 1}`.
- **Display colour order and inversion**: we use Arduino_GFX's default ST7789 init with
  `ips=false`. Red/blue swapped => RGB/BGR order; dark/negative colours => inversion
  (Settings -> Invert colors toggles it at runtime).
- **Display rotation**: `DISPLAY_ROTATION 3` matches the vendor/Bruce config; unverified with Arduino_GFX.
- **SPI speed**: display at 40 MHz (`DISPLAY_SPI_HZ`), SD at 20 MHz, touch at 2.5 MHz.
  Drop the display to 20 MHz if there are glitches.
- **Which USB-C port carries serial logs**: we log to the C5's native USB (USB-Serial-JTAG,
  `ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`). The other port is a CH340 on
  UART0 (GPIO 11/12). Which physical port is which is unconfirmed.
- **I2C pin order**: schematic says SDA 9 / SCL 8, vendor config says the reverse.
  Bring-up mode reports which one finds the AHT20.
- **PSRAM**: expected 8 MB, and the screen framebuffer should land in PSRAM.
- **5 GHz scanning**: `WiFi.setBandMode(WIFI_BAND_MODE_AUTO)` compiles in; not yet seen working.
- **BOOT button at power-on**: assumed to enter download mode (Espressif strapping);
  confirm on the board.

## Roadmap (don't start until the owner confirms hardware works)

AHT20 environment stat -> 802.15.4 (Zigbee/Thread) passive scanner -> battery support
(LiPo -> charger/boost -> 5 V on P5 pin 1, MAX17048-style fuel gauge on I2C) ->
account pairing and summary sync.
