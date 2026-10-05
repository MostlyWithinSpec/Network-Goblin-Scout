# Battery power for NG Scout

The NM-CYD-C5 has no charger and no battery monitoring, so going portable needs a small
add-on. This is the plan, based on the v1.0 schematic. **Nothing here has been tested on
hardware yet**: measure before you trust it.

## What the board already does with power

```
USB-C VBUS ──D1 (diode)──┬── 5V rail ──┬── AMS1117-3.3 ── 3.3V_ESP (ESP32-C5)
                         │             └── AMS1117-3.3 ── 3.3V (display, SD, touch...)
P5 pin 1 ──Q1 (AO3401A)──┘
```

- **D1** means anything you put on the 5 V rail can't flow back into your computer's USB port.
- **Q1** is a P-channel MOSFET with its gate tied to ground. That's reverse-polarity
  protection. Once it's on it conducts both ways, so **P5 pin 1 can be used as a 5 V input**,
  not just an output for the GPS.
- The **AMS1117** regulators need about **4.4 V or more** on the 5 V rail to produce a clean 3.3 V.

## Two rules (break these and you'll cook something)

1. **Never wire a LiPo straight to the 3V3 pad.** A full cell is 4.2 V, above the ESP32-C5's
   3.6 V maximum, and it would also push current backwards into both regulators.
2. **Never wire a LiPo straight to the 5 V pad.** At 3.7 V the regulators drop out and the board
   browns out. You need a **boost converter to 5 V**.

## Recommended setups

### A. Best: Adafruit PowerBoost 1000C + LiPo

- **Charger + 5 V boost** in one, with *load sharing* (it runs the board from USB while it charges),
  a low-battery pin, and an enable pin for an on/off switch.
- **Battery:** 1S LiPo, 1200–2500 mAh, **with a protection circuit**, JST-PH connector.
- **Wiring:** PowerBoost `5V` → **Schottky diode** (SS14 or 1N5817, stripe towards the board)
  → board **5V pad** (or P5 pin 1); PowerBoost `GND` → board GND. Slide switch between the
  PowerBoost `EN` and `GND` pins.
- **Why the diode:** when the board's own USB is plugged in, its 5 V rail (~4.7 V through D1)
  would otherwise push current backwards into the boost converter's output. The diode costs
  ~0.3 V, leaving ~4.7 V on the rail, which is still enough for the regulators.
- **Charge** through the PowerBoost's own USB port.

### B. Budget: IP5306 "power bank" module (USB-C)

- Cheap all-in-one charger + 5 V boost, often with 4 LEDs showing charge level.
- Same wiring and diode as above.
- **Catch:** IP5306 boards switch themselves off below ~45 mA. NG Scout normally draws more than
  that, so it should stay on, but some modules need a button press to wake.

### C. Most accurate: SparkFun Battery Babysitter + a 5 V boost

- The **Battery Babysitter** charges the LiPo (BQ24075) and has a **BQ27441 fuel gauge** that counts
  charge in and out, so its percentage is better than a voltage-based guess. The firmware supports it
  (v0.4.1+): it's detected automatically on CN1 at I2C address 0x55.
- **It has no 5 V boost.** On battery its output is the cell voltage (3.0-4.2 V): too low for the
  5V pad, too high for the 3V3 pad. Add a **5 V step-up** (e.g. Pololu U3V40F5 or U1V11F5):
  Babysitter output → boost IN, boost 5V OUT → **Schottky (SS14)** → board 5V pad, all GNDs together.
- **I2C:** SDA/SCL to CN1 pins 2/3, GND to pin 4. If the Babysitter's I2C pull-ups need a supply pin,
  feed it **3.3 V from CN1 pin 1**, never the battery: the ESP32-C5's pins take 3.6 V at most. Check
  the Babysitter's hookup guide for which pin that is.
- Set your cell's capacity in `include/config.h`: `BATTERY_CAPACITY_MAH` (default 2000). The firmware
  writes it into the gauge at boot when it differs.

## Battery percentage on screen: MAX17048 fuel gauge

- (Or the BQ27441 on a Battery Babysitter, option C above.) A MAX17048 breakout (Adafruit #5580, SparkFun, or a generic one) reads the cell and reports
  **percentage, voltage, and whether it's charging** over I2C (address 0x36).
- **Connect it to CN1:** pin 1 = 3.3 V, pin 2 = SDA (IO9), pin 3 = SCL (IO8), pin 4 = GND.
  ⚠️ CN1 is **not** in the Qwiic/STEMMA QT pin order (GND, 3V3, SDA, SCL), so make a cable
  by pin name, not by plugging two "matching" connectors together.
- The gauge's battery pins go across the LiPo (+ and −).
- **Firmware (done in v0.4, untested on hardware):** detected automatically at boot. Battery %
  and a charging bolt in the status bar, a droopy goblin with low-battery quips at 15 % or less,
  a `BATT` line in diagnostics, and achievements ("Unplugged": an hour on battery).

## How long will it last?

Rough guess until someone measures it: the board probably draws 150–250 mA at 5 V with the
screen on and all radios scanning. On a 2000 mAh cell, through a ~85 % efficient boost, that's
roughly **6–9 hours**. Pocket mode (screen off) should stretch it.

A cheap USB power meter between the charger and the board will give real numbers. Please
report them, and I'll tune the estimates and add a battery-saver mode.
