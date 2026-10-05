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
- CI: `.github/workflows/build.yml` builds every push and uploads the binaries as an artifact,
  runs the PC unit tests and uploads rendered UI screenshots.

### PC-side tools (no board needed)

```sh
g++ -std=c++17 -Wall -Wextra -I src test/test_ieee802154.cpp -o /tmp/t && /tmp/t   # 802.15.4 parser
g++ -std=c++17 -Wall -Wextra -I src test/test_peer.cpp -o /tmp/t && /tmp/t         # beacon codec
sh tools/preview/run.sh        # renders the real UI to tools/preview/out/*.png (needs Pillow)
python3 tools/gen_assets.py    # regenerate src/ui/assets/* from assets/ (logo, fonts)
```

Use the preview to check any UI change before handing a build to the owner: it is the only way to
see the screens without the hardware. Keep `src/ui/` free of Arduino/hardware calls so it keeps working.

**The ESP32-C5 has no FPU** (`-march=rv32imac`): every float op is a slow software call, and the PC
preview hides that. No float maths in per-pixel loops; floats are fine once per shape/frame.
Measure UI cost on the real CPU type before shipping drawing changes:

```sh
pip install unicorn pyelftools
sh tools/bench/build.sh && python3 tools/bench/run.py tools/bench/bench.elf
```

It cross-compiles the UI for rv32imac, runs it in an emulator and prints instructions per frame
plus the hottest functions and soft-float callers. (Instructions only: PSRAM latency comes on top.)
v0.2.0 measured 31 M instr/frame on Home (owner saw 200 ms render); v0.2.2 is ~3.4 M.

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
   Same for `Radio` enum values and `NG_SAVED_COUNTERS` names (both feed stored data).
7. **The only transmission is the goblin beacon** (`src/social/`): non-connectable BLE advert, random
   per-boot address, no user data, user-toggleable. Never add probe requests, active scans or
   802.15.4 transmissions.

## Layout

```
include/board.h, config.h, ng_log_level.h
src/main.cpp         setup + non-blocking loop
src/hal/             Display (PSRAM canvas), Touch (XPT2046), Storage (SD), Gps, Fx (LED/speaker), Aht20
src/scanners/        Scanner interface, WifiScanner, BleScanner, ThreadScanner (802.15.4), ScanManager,
                     Ieee802154Frame.h (pure MAC header parser)
src/social/          Peer identity (NVS) + goblin BLE beacon; PeerCodec.h = pure wire format
src/core/            Engine (sighting -> XP -> achievements -> persistence), Achievements, SeenStore, HashSet64
src/ui/              Pure UI: Ui (screens/overlays/input), Companion (animated logo goblin), Widgets, Theme,
                     Model (per-frame snapshot from main.cpp), gfx/ (renderer), assets/ (generated)
                     Sprites.cpp is the one device-only file (loads SD packs)
src/diag/            Hardware bring-up mode
test/, tools/        PC unit tests, asset generator, UI preview
```

## Hardware bring-up mode

Power on, then **press BOOT within 1.5 s** (while the splash says "press BOOT now").
Holding BOOT *during* power-on may enter the ESP32-C5's USB flashing mode instead.
The screen shows, and serial prints every 2 s (lines prefixed `[diag]`): chip/flash/PSRAM,
framebuffer location, SD write test, raw touch, Wi-Fi/BLE scan counts, GPS NMEA counters,
AHT20 reading (tries both I2C pin orders), I2C scan, BOOT state, LED colour cycle,
colour swatches and corner marks. Press RESET to exit. Counts only — no MACs/SSIDs/coordinates.

## Hardware status (from flash reports — update as results come in)

Confirmed working on the owner's board (v0.1 bring-up + main app):
display, colour order, rotation, RGB LED, PSRAM (8 MB, framebuffer in PSRAM), microSD, touch
(both axes inverted, `TOUCH_INVERT_X/Y 1`), Wi-Fi on 2.4 and 5 GHz, BLE scanning, on-screen tabs.
BOOT held during power-on/flashing = download mode (confirmed), hence the 1.5 s window.

Not fitted / not present: **AHT20 (U28) is not populated** on the owner's board (schematic only);
the environment stat was dropped. GPS and speaker not connected yet.

v0.2 on hardware: boots, runs, all radios scanning (owner report). GUI "a tiny bit laggy" →
v0.2.1 adds Turbo display + changed-areas-only flush. Owner log (v0.2.1): crystal 48 MHz,
render ~200 ms, push 33 ms, 2-4 fps → render was the bottleneck (soft-float). v0.2.2 rewrote the
renderer in integer maths (~9x fewer instructions); awaiting the next `ui:` line.

Untested on hardware: goblin beacon + encounters. Test with one board and a phone: nRF Connect →
Advertiser → Manufacturer Data, company ID `0xFFFF`, data `4E470178563412 0C00C8004D6F636B`
(a level-12 goblin called "Mock").

Still unknown:
- **Touch min/max calibration** (`TOUCH_RAW_*`): unverified; vendor TFT_eSPI calibration is `{225, 3413, 403, 3334, 1}`.
- **SPI speed**: on the C5 the Arduino core clocks SPI from the crystal (40 or 48 MHz, logged at boot), so
  that is the ceiling; "40 MHz" may really be 24 MHz on a 48 MHz crystal. Turbo display asks for the max.
  Going past the crystal would need the PLL clock source and per-device divider fixes (SD, touch), or
  IDF spi_master/esp_lcd with DMA for the whole shared bus. Neither done yet.
- **Which USB-C port carries serial logs**: native USB (USB-Serial-JTAG) vs CH340 on UART0 (GPIO 11/12).

## Roadmap

Done in v0.2 (awaiting hardware test): 802.15.4 scanner, goblin encounters, GUI overhaul, 110 achievements.
Next: battery support (LiPo -> charger/boost -> 5 V on P5 pin 1, MAX17048-style fuel gauge on I2C)
-> account pairing and summary sync.
