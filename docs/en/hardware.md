# Hardware, pins and wiring

## Bill of materials

| Qty | Part | Notes |
|---|---|---|
| 1 | **ESP32-2432S028R ("CYD")**, 2.8" 320x240 touch | The panel (USB powered). `firmware/controller-cyd` targets this board |
| 2 | ESP32-S3 Super Mini | Hub and bed node |
| 2 | 1-channel 5 V relay module | **Active-low** type (relay pulls in when `IN` = LOW). Opto-isolated. Check with `firmware/tools/relay-test` |
| 1 | Aluminium foil tape + wire | The hub's touch switch |
| 3 | 5 V USB adapters (≥1 A) + cables | Hub, bed node and the CYD panel. The nodes also power the relay through the ESP's 5 V pin |
| — | Closed insulated enclosure, terminal blocks, wire (≥0.75 mm²) | For the mains part |
| — | (recommended) Multimeter | To identify relay terminals; a non-contact voltage tester to find the phase wire |

## Pins

### Hub and bed node (ESP32-S3 Super Mini)

| Signal | Hub | Bed node |
|---|---|---|
| Relay `IN` | **GPIO 5** | **GPIO 5** |
| Relay `VCC` | `5V` | `5V` |
| Relay `GND` | `GND` | `GND` |
| Touch foil | **GPIO 4** (touch pad T4) | — |

```
 ESP32-S3 Super Mini            Relay module (5 V, active-low)
   5V     ------------------->  VCC
   GND    ------------------->  GND
   GPIO5  ------------------->  IN
   GPIO4  ---- wire ---- [ foil tape ]      (hub only)
```

Pins are `#define`s at the top of `firmware/hub/src/main.cpp` and `firmware/bed-node/src/main.cpp`. Avoid GPIO 0, 3, 45, 46 (boot/strapping) and 19, 20 (USB). Power the relay `VCC` from **5V, not 3V3** (the coil draws 70–90 mA). Before switching the pin to output the firmware sets it to the "off" level, so the relay does not blip at power-up.

### Panel (CYD, ESP32-2432S028R)

A ready-made board with a 2.8" 320x240 touch screen. Its pins are fixed on the board, no wiring needed. `firmware/controller-cyd` is written for these pins:

| Part | Pin |
|---|---|
| Display (ILI9341, HSPI) | MOSI 13, MISO 12, SCLK 14, CS 15, DC 2 |
| Backlight (PWM) | GPIO 21 |
| Touch (XPT2046, separate SPI bus) | CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36 |
| Light sensor (LDR) | GPIO 34 (works inverted: more light = lower voltage, the firmware compensates) |

Notes:
- The panel works with TFT_eSPI's `ILI9341_2_DRIVER` variant plus inverted colours (set in `platformio.ini`). If colours are inverted or the picture is garbled, check this first.
- The board has no battery/charger: it runs from USB (a ≥1 A adapter is enough). So there is no deep sleep; the screen only turns the backlight off.
- On first boot you touch four corner targets to calibrate (repeatable from Settings → STATUS).
- The board's MAC must be set as `NOW_CONTROLLER_MAC` in the `now_config.h` of the hub and bed node as well.

## Relay terminals: which is which?

Most modules have three screw terminals **NC – COM – NO** (order varies, often unlabeled). Identify them with a multimeter, **with nothing connected to mains**:

1. Multimeter in continuity (beep) mode. Relay **unpowered**: test the terminals in pairs. The pair that **beeps = COM + NC**. The one that doesn't beep is **NO**.
2. Power the module (`VCC` 5 V, `GND`) and touch `IN` to `GND` to energize it (click, module LED on). Test again. Now the **beeping pair = COM + NO**.
3. The terminal that beeps in **both** cases is **COM**. The one that beeps only when energized is **NO**, only when idle is **NC**. Mark them with a pen.

## Mains wiring

> **⚠️ Mains voltage can kill.** If you are not experienced with it, have an electrician do this part. Nothing here is professional electrical advice or a guarantee.

Rules:
- **Switch the breaker off** and unplug the lamp before touching anything. Do not restore power until everything is finished and enclosed.
- Cut **only one conductor (the phase/live wire)** of the lamp's cord. The **plug-side end goes to `COM`**, the **lamp-side end to `NO`**. Leave `NC` empty. The **neutral (and earth, if any) never goes through the relay**, it runs straight to the lamp.
  ```
  Plug phase ---------> COM   (relay)   NO ---------> lamp phase
  Plug neutral --------------------------------------> lamp neutral
                                NC: leave empty
  ```
- Use **NO**, not NC: when the ESP is off or power returns after an outage, the lamp stays **off**. If you wire NC the lamp starts on and the on/off states in the software appear inverted (do **not** "fix" this in software).
- You cannot tell phase from neutral by eye: use a **non-contact voltage tester**, or simply **always unplug the lamp when changing the bulb** (if you cut the neutral by mistake, the lamp socket stays live even when "off").
- Put every mains connection in a **closed, insulated enclosure**; tighten the screws and give each wire a gentle tug; provide strain relief; keep low-voltage wires away from mains wires. Never leave bare copper exposed.
- Check the relay's rating printed on it (e.g. 10 A / 250 VAC); an LED bulb is far below that.
- Do your first tests with **only the 5 V side** connected (listen for the click), and only then connect the mains side.
- Leave the lamp's own switch **on**; switching is done by the relay.

## Power

- **Hub and bed node:** one 5 V ≥1 A USB adapter + one USB-C cable each. The ESP feeds the relay's `VCC` from its own `5V` pin, no second cable. Never feed 5 V to the `5V` pin and USB at the same time.
- **Panel (CYD):** 5 V over USB (a ≥1 A adapter). The board has no battery.

## Hub touch switch

The hub calibrates a baseline during the first 3 s after boot (**do not touch the foil then**), and treats a deviation of `TOUCH_DELTA_PCT` % as a touch (3 consecutive readings, then 400 ms lockout), with slow drift tracking. On the ESP32-S3 the reading **rises** on touch; on a classic ESP32 it **falls** (`TOUCH_RISES_ON_TOUCH` handles this). To tune: set `TOUCH_DEBUG` to `1`, watch the values in the serial monitor, adjust `TOUCH_DELTA_PCT`.
