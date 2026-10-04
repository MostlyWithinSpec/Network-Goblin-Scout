#pragma once
// Tunables. Anything hardware-variant-specific that might need tweaking lives here.

#define NG_FW_VERSION           "0.1.0"
#define NG_DATA_DIR             "/scout"
#define NG_SPRITE_DIR           "/sprites"

// ---- Display --------------------------------------------------------------
#define DISPLAY_SPI_HZ          40000000   // drop to 20000000 if you see glitches
#define DISPLAY_ROTATION        3          // landscape, matches vendor/Bruce config
#define UI_FRAME_MS             100        // ~10 fps redraw

// ---- Touch (XPT2046 raw 12-bit values) ------------------------------------
// Use Settings -> Touch test to read raw values at the corners, then adjust.
#define TOUCH_RAW_X_MIN         185
#define TOUCH_RAW_X_MAX         3700
#define TOUCH_RAW_Y_MIN         250
#define TOUCH_RAW_Y_MAX         3800
#define TOUCH_SWAP_XY           1
#define TOUCH_INVERT_X          0
#define TOUCH_INVERT_Y          0
#define TOUCH_MIN_PRESSURE      300
#define TOUCH_SPI_HZ            2500000

// ---- Scanning -------------------------------------------------------------
#define WIFI_SCAN_MS_PER_CHAN   120        // passive dwell per channel
#define BLE_SCAN_SECONDS        4
#define SCAN_REST_MS            1500

// ---- Persistence ----------------------------------------------------------
#define STATE_SAVE_INTERVAL_MS  60000

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
#define XP_ACHIEVEMENT          25

// ---- Sprites --------------------------------------------------------------
#define SPRITE_BOX              128        // companion drawing area (px)
#define SPRITE_TRANSPARENT      0xF81F     // magenta = transparent
