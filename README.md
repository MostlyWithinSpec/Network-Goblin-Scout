# Network Goblin Scout — firmware

Passive wireless discovery companion for the **NM-CYD-C5** (ESP32-C5, dual-band Wi-Fi 6, BLE 5, 802.15.4).
Scan → discover → earn XP → unlock achievements → level up your goblin. Works fully offline.

## Build & flash

1. VS Code + PlatformIO. Open this folder.
2. First build downloads the **pioarduino** platform (Arduino core 3.3.x, needed for ESP32-C5). It's large; give it a few minutes.
3. Plug in either USB-C port. `PlatformIO: Upload`, then `Monitor`.
   Logs go to the **native ESP32-C5 USB port** by default. To get logs on the CH340 port, set
   `ARDUINO_USB_MODE` and `ARDUINO_USB_CDC_ON_BOOT` to `0` in `platformio.ini`.
4. Copy the contents of `sd_card/` to the root of a FAT32 microSD card.

No toolchain? Every push builds in GitHub Actions: open the run under **Actions**, download the
`ng-scout-<commit>` artifact and flash `firmware.factory.bin` at `0x0` (see `FLASHING.txt` inside).

### Hardware bring-up mode

Power on, then **press BOOT within 1.5 s** while the splash says "press BOOT now for diagnostics".
(Holding BOOT *while* powering on can put the ESP32-C5 into USB flashing mode instead.)
One screen, mirrored to serial every 2 s, shows PSRAM, SD, raw touch, Wi-Fi/BLE scan counts,
GPS, the AHT20 sensor, an I2C scan, LED colours and display colour swatches. Press RESET to exit.

## First-boot checklist

| Check | If it's wrong |
|---|---|
| Screen shows the splash, then the goblin | Colors inverted: Settings → Invert colors. Mirrored/rotated: change `DISPLAY_ROTATION` in `include/config.h` |
| Tapping the tabs works | Settings → Touch test, tap the four corners, then adjust `TOUCH_*` in `config.h` (swap/invert/min/max) |
| Status bar shows green **SD** | Card must be FAT32; progress won't save without it |
| Wi-Fi count climbs, 5 GHz channels light up on the Stats strip | Tell Claude what the serial log shows |
| BLE count climbs | BLE only counts devices with **stable** addresses (see privacy notes) |

The **BOOT** button toggles "pocket mode" (screen off, scanning continues). Tap the screen to wake.

## Layout

```
include/board.h          Pin map from the NM-CYD-C5 v1.0 schematic
include/config.h         Tunables: display, touch calibration, scan timing, XP rules
src/main.cpp             Wiring + main loop (non-blocking)
src/hal/                 Display (PSRAM canvas), Touch (XPT2046), Storage (SD), Gps, Fx (LED/speaker)
src/scanners/            Scanner interface + WifiScanner, BleScanner, ThreadScanner (v1.1 stub), ScanManager
src/core/                Engine (discovery → XP → achievements → persistence), Achievements, SeenStore, HashSet64
src/ui/                  Ui (screens/tabs/toasts), Companion (pet state machine + built-in goblin), Sprites (BMP packs)
sd_card/                 Copy to microSD root — includes the example `pixel_goblin` sprite pack
```

**Adding a radio** = implement `Scanner` (begin/start/poll) and `scans.add()` it. The engine doesn't care where a `Sighting` came from.

**Adding an achievement** = append a line to `ACHIEVEMENTS[]` in `core/Achievements.cpp`. Never reorder or rename ids (they're saved on SD and will be synced later).

## What's on the SD card

```
/scout/salt.bin      Random per-device salt (don't share)
/scout/state.json    XP, counters, channels, achievements, settings
/scout/wifi.csv      id, ssid_id, channel, auth, rssi, unix_time, lat, lon   (one line per new BSSID)
/scout/ssids.txt     ids of unique SSIDs
/scout/ble.csv       id, rssi, unix_time                                     (one line per new stable BLE device)
/scout/cells.txt     ids of ~5 km exploration cells visited
/sprites/<pack>/     metadata.json + BMP frames
```

### Privacy notes

- Scanning is **passive**: Wi-Fi listens for beacons (no probe requests), BLE never sends scan requests or connects.
- MAC addresses and SSIDs are stored only as **salted SHA-256 ids**, never in plain text. GPS coordinates stay on the card.
- BLE devices using rotating private addresses (most phones) count as *sightings*, not unique devices.

## Sprite packs

`/sprites/<name>/metadata.json`:

```json
{
  "name": "Pixel Goblin", "author": "you", "version": "1.0",
  "states": {
    "idle": ["idle_01.bmp", "idle_02.bmp"],
    "scanning": ["scan_01.bmp", "scan_02.bmp"],
    "happy": ["happy_01.bmp"],
    "excited": [], "level_up": [], "achievement": [], "searching": [],
    "sleep": [], "low_battery": [], "offline": [], "uploading": [], "sync_complete": []
  }
}
```

- BMP, 24-bit, 32-bit (alpha) or 16-bit RGB565, uncompressed. Max 128×128.
- Sprites 64×64 or smaller are drawn at 2× (pixel-art friendly).
- Magenta `#FF00FF` is transparent.
- Missing states fall back to `idle`; a missing or broken pack falls back to the built-in goblin.
- `discovered` also accepts `happy`; `level_up` also accepts `levelup`; `sleep` also accepts `sleeping`.

## Status / next steps

- [x] Display, touch, SD, GPS (auto-detect), RGB LED, speaker
- [x] Passive dual-band Wi-Fi + BLE scanning, persistent dedupe
- [x] XP, levels, 19 achievements, daily streak (needs GPS time), exploration cells
- [x] Companion states, built-in goblin, SD sprite packs
- [ ] Battery: board has no charger/sense. Plan: LiPo → charger/boost → 5V on P5 pin 1; I2C fuel gauge (e.g. MAX17048) on CN1 since all ADC pins are used
- [ ] AHT20 temp/humidity (on-board, I2C 8/9) for a fun "environment" stat
- [ ] 802.15.4 (Zigbee/Thread) scanner — v1.1
- [ ] Account pairing + summary sync + website — later phase
