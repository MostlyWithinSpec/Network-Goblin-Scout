#pragma once
// Tunables. Anything hardware-variant-specific that might need tweaking lives here.

#define NG_FW_VERSION           "0.4.0"
#define NG_DATA_DIR             "/scout"
#define NG_SPRITE_DIR           "/sprites"

// Where the share card's QR code points. Change it to whatever you want people to find.
#define NG_SHARE_URL            "https://github.com/MostlyWithinSpec/Network-Goblin-Scout"

// ---- Display --------------------------------------------------------------
#define DISPLAY_SPI_HZ          40000000   // "Turbo display" off
// "Turbo display" on: asks for the maximum; the ESP32-C5 Arduino core clocks SPI from the
// crystal, so this really means "crystal speed" (40 or 48 MHz). Toggle in Setup.
#define DISPLAY_SPI_HZ_FAST     80000000
#define DISPLAY_ROTATION        3          // landscape, matches vendor/Bruce config
#define UI_FRAME_MS             40         // ~25 fps target (a full-screen SPI push takes ~31 ms at 40 MHz)

// ---- Touch (XPT2046 raw 12-bit values) ------------------------------------
// Use Settings -> Touch test to read raw values at the corners, then adjust.
#define TOUCH_RAW_X_MIN         185
#define TOUCH_RAW_X_MAX         3700
#define TOUCH_RAW_Y_MIN         250
#define TOUCH_RAW_Y_MAX         3800
#define TOUCH_SWAP_XY           1
// Both axes inverted: confirmed on hardware (crosshair landed opposite the finger),
// and matches the vendor's rotation-3 handling in their Bruce port.
#define TOUCH_INVERT_X          1
#define TOUCH_INVERT_Y          1
#define TOUCH_MIN_PRESSURE      300
#define TOUCH_SPI_HZ            2500000

// ---- Scanning -------------------------------------------------------------
#define WIFI_SCAN_MS_PER_CHAN   120        // passive dwell per channel
#define BLE_SCAN_SECONDS        4
#define IEEE802154_DWELL_MS     250        // per channel, 16 channels (11-26) = 4 s per sweep
#define SCAN_REST_MS            1500

// ---- Persistence ----------------------------------------------------------
#define STATE_SAVE_INTERVAL_MS  30000      // plus within 3 s of level-ups, achievements, quests

// ---- GPS ------------------------------------------------------------------
#define GPS_BAUD                9600
#define GEO_CELL_DEG            0.05       // ~5.5 km "exploration cell"

// ---- XP rules -------------------------------------------------------------
#define XP_NEW_NETWORK          1
#define XP_NEW_ENTERPRISE       5
#define XP_NEW_CHANNEL          10
#define XP_NEW_CELL             20
#define XP_DAILY_BONUS          50
#define XP_NEW_BLE              1
#define XP_ACHIEVEMENT          25         // x (1 + tier): bronze 25 ... legendary 100
#define XP_NEW_154              2          // new 802.15.4 device
#define XP_NEW_PAN              15         // new Zigbee/Thread network
#define XP_NEW_PEER             100        // met a new goblin
#define XP_PEER_REUNION         25         // met a known goblin again
#define XP_BOARD_CLEARED        100        // finished all three quests
#define XP_SNIFF_WIN            60         // goblin sniff-off: bigger hoard
#define XP_SNIFF_DRAW           40
#define XP_SNIFF_LOSE           20         // everyone gets something for showing up
#define SNIFF_COOLDOWN_MS       (6UL * 60 * 60 * 1000)  // one sniff-off per goblin per 6 h

// ---- Quests & needs -------------------------------------------------------
#define QUEST_COOLDOWN_MIN      30         // minutes of scanning before a new board after clearing one
#define HUNGER_PER_MIN          8          // 0..1000: empty to starving in ~2 h without discoveries
#define BOREDOM_PER_MIN         5          // 0..1000: ~3.3 h without anything new or fun

// ---- Goblin encounters ----------------------------------------------------
#define PEER_REVISIT_MS         (15UL * 60 * 1000)  // same goblin counts again after 15 min apart
#define PEER_TOGETHER_MS        (2UL * 60 * 1000)   // "nearby" = seen in the last 2 min

// ---- Sprites --------------------------------------------------------------
#define SPRITE_BOX              128        // companion drawing area (px)
#define SPRITE_TRANSPARENT      0xF81F     // magenta = transparent
