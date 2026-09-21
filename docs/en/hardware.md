# Hardware, pins and wiring

## Bill of materials

| Qty | Part | Notes |
|---|---|---|
| 1 | Waveshare **ESP32-S3-Touch-AMOLED-1.64** | The remote. This repo targets the **V1 pinout** (test yours with `firmware/tools/board-check`) |
| 2 | ESP32-S3 Super Mini | Hub and bed node |
| 2 | 1-channel 5 V relay module | **Active-low** type (relay pulls in when `IN` = LOW). Opto-isolated. Check with `firmware/tools/relay-test` |
| 1 | LiPo cell **with protection circuit (PCM)**, MX1.25 plug | For the remote. The board has **no** over-discharge/short-circuit protection. Check the plug polarity |
| 1 | Aluminium foil tape + wire | The hub's touch switch |
| 2 | 5 V USB adapters (≥1 A) + USB-C cables | Hub and bed node. They also power the relay through the ESP's 5 V pin |
| — | Closed insulated enclosure, terminal blocks, wire (≥0.75 mm²) | For the mains part |
| — | (optional) WiFi powerline adapter / repeater | If the hub's WiFi signal is weak |
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

### Remote (Waveshare, V1)

| Signal | V1 GPIO | V2 |
|---|---|---|
| LCD CS | **9** | 46 |
| LCD CLK / D0 / D1 / D2 / D3 | 10 / 11 / 12 / 13 / 14 | verify |
| LCD RST | 21 | verify |
| Touch SDA / SCL (I²C 0x38, FT3168) | **47 / 48** | different |
| Touch INT | **none** | GPIO 18 |
| IMU QMI8658 | I²C 0x6B | — |
| Battery/system voltage ADC | GPIO 4 | — |

The board's external header **P2** (V1 schematic): pin 8 = 3V3, pin 9 = VBAT, pin 10 = GND, pin 11 = **USB_5V**. Feeding 5 V into pin 11 powers the board and charges the battery exactly like USB does; the line is **unprotected**, so add a fuse/TVS/reverse protection for any exposed contacts (e.g. a magnetic dock) and keep the source ≤ 5.5 V.

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
- **Remote:** LiPo and/or USB-C or a dock. While running on battery the system rail is ≤ 4.2 V, with external 5 V it is about 4.5–4.9 V; the charging icon and the Status page use this (see [../waveshare-amoled-notes.md](../waveshare-amoled-notes.md), Turkish).
- **Magnetic dock idea:** two nickel-plated neodymium magnets can serve as both holder and contacts. Mount them with opposite poles facing out so the device cannot be attached the wrong way (magnets repel). Wire the contacts to P2 pins 11 (`USB_5V`) and 10 (`GND`) through a polyfuse and TVS diode. Keep magnets and metal contacts **away from the ESP32 module's antenna**. The IMU has no magnetometer, so magnets do not disturb orientation.

## Hub touch switch

The hub calibrates a baseline during the first 3 s after boot (**do not touch the foil then**), and treats a deviation of `TOUCH_DELTA_PCT` % as a touch (3 consecutive readings, then 400 ms lockout), with slow drift tracking. On the ESP32-S3 the reading **rises** on touch; on a classic ESP32 it **falls** (`TOUCH_RISES_ON_TOUCH` handles this). To tune: set `TOUCH_DEBUG` to `1`, watch the values in the serial monitor, adjust `TOUCH_DELTA_PCT`.
