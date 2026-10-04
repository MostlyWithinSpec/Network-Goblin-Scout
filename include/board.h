#pragma once
// NM-CYD-C5 v1.0 pin map — taken from SCH_NM-CYD-C5-v1.0.pdf (RockBase-iot/NM-CYD-C5)

// Shared SPI bus: display, touch and microSD all live on it
#define PIN_SPI_SCK     6
#define PIN_SPI_MOSI    7
#define PIN_SPI_MISO    2

// ST7789 2.8" 240x320 (TFT_RS on the schematic = DC)
#define PIN_TFT_CS      23
#define PIN_TFT_DC      24
#define PIN_TFT_RST     -1   // tied to the ESP32-C5 reset line
#define PIN_TFT_BL      25   // via AO3400A, PWM-able

// XPT2046 resistive touch
#define PIN_TOUCH_CS    1
#define PIN_TOUCH_IRQ   3    // PENIRQ, 10k pull-up (R3)

// microSD
#define PIN_SD_CS       10

// Misc on-board
#define PIN_RGB_LED     27   // WS2812C through a level shifter (strapping pin)
#define PIN_SPEAKER     26   // SPEAK_IN -> SC8002B amp -> "VO+/VO-" speaker header
#define PIN_BOOT_BTN    28   // BOOT button, active LOW, usable after boot

// P5 header (LP UART) — GPS goes here. Pin 1 is 5V through a P-FET.
#define PIN_GPS_RX      4    // ESP RX  <- GPS TX
#define PIN_GPS_TX      5    // ESP TX  -> GPS RX

// CN1 I2C (shared with on-board AHT20 temp/humidity sensor)
#define PIN_I2C_SDA     9
#define PIN_I2C_SCL     8

#define SCREEN_W        320
#define SCREEN_H        240
