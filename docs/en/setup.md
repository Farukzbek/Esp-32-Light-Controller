# Step-by-step setup guide

Follow the stages **in order**. Every stage ends with a "✅ Check" so you know it worked before you move on. Mains voltage (220/230 V) is connected **last**, after everything works on the bench.

Estimated time: 1–2 hours (the first build downloads toolchains and takes a few minutes).

| Stage | What | Needs |
|---|---|---|
| 0 | Install tools, download the project | Computer |
| 1 | Read the boards' MAC addresses, create `now_config.h` | 3 boards |
| 2 | Bed node on the bench (relay click test) | S3 Super Mini + relay |
| 3 | Desk node (hub): touch switch + relay | S3 Super Mini + relay + foil tape |
| 4 | Panel (CYD) | CYD board |
| 5 | End-to-end test (still no mains) | everything |
| 6 | Connect the lamps (mains) | see [hardware.md](hardware.md#mains-wiring) — read the safety section first |

---

## Stage 0: Tools

1. **Install PlatformIO Core** (needs Python 3.8+):
   ```bash
   pip install -U platformio        # any OS
   # macOS alternative:  brew install platformio
   # or use the "PlatformIO IDE" extension of VS Code
   ```
   Check: `pio --version` prints a version.
2. **Download the project:**
   ```bash
   git clone https://github.com/Farukzbek/Nowlight.git
   cd Nowlight
   ```
3. **Find the serial port of a board** (plug in **one** board, then):
   | OS | Command / where | Looks like |
   |---|---|---|
   | macOS | `ls /dev/cu.usbmodem*` | `/dev/cu.usbmodem1101` |
   | Linux | `ls /dev/ttyACM*` (add yourself to the `dialout` group once: `sudo usermod -aG dialout $USER`, then log out/in) | `/dev/ttyACM0` |
   | Windows | Device Manager → Ports (COM & LPT) | `COM3` |

   The ESP32-S3 boards use the chip's **native USB**, no extra driver is normally needed.

✅ Check: `pio --version` works and you can see a port when a board is plugged in.

## Stage 1: MAC addresses and `now_config.h`

ESP-NOW pairs devices by MAC address, so you need the three addresses **before** flashing. Reading them works on a brand-new board without any firmware:

```bash
pip install esptool
esptool --port <PORT> read-mac
```

Plug the boards **one at a time** and write down which MAC belongs to which board:

| Board | Role | MAC (example) |
|---|---|---|
| CYD (ESP32-2432S028R) | Panel (`NOW_CONTROLLER_MAC`) | `A0:B1:C2:D3:E4:F5` |
| S3 Super Mini #1 | Hub (`NOW_HUB_MAC`) | … |
| S3 Super Mini #2 | Bed node (`NOW_BED_MAC`) | … |

Then create your config (this file is git-ignored, it never leaves your computer):

```bash
cp firmware/shared/now_config.example.h firmware/shared/now_config.h
```

Open `now_config.h` and set:
- the three MAC addresses (**UPPER CASE**, colon separated),
- `NOW_CHANNEL`: the ESP-NOW channel (1–13, default 1) where the three devices meet. No WiFi/router needed; avoid your router's 2.4 GHz channel if you can,
- `NOW_PASSWORD`: any passphrase, the same for all three devices. Change it from the default.

✅ Check: building without this file shows a clear `#error "now_config.h yok…"` message; with the file, `pio run` works.

## Stage 2: Bed node on the bench

1. **Wire only the low-voltage side** of a relay module to an ESP32-S3 Super Mini:

   | S3 Super Mini | Relay module |
   |---|---|
   | `5V` | `VCC` |
   | `GND` | `GND` |
   | `5` (GPIO5) | `IN` |

   **Do not connect anything to the relay's screw terminals yet.**
2. **Relay polarity test:**
   ```bash
   cd firmware/tools/relay-test
   pio run -t upload --upload-port <PORT>
   pio device monitor -b 115200 -p <PORT>
   ```
   The monitor prints `GPIO5 = LOW` / `GPIO5 = HIGH` every 2 s. With the usual **active-low** relay module the relay **clicks (and its LED lights) while it says LOW**. If it clicks on **HIGH** instead, your module is active-high: set `RELAY_ACTIVE_LOW` to `0` in `firmware/hub/src/main.cpp` and the equivalent `RELAY_ON`/`RELAY_OFF` in `firmware/bed-node/src/main.cpp`.
3. **Flash the bed node:**
   ```bash
   cd ../../bed-node
   pio run -t upload --upload-port <PORT>
   pio device monitor -b 115200 -p <PORT>
   ```
   It boots with the relay **off**, and prints `YATAK DUGUMU  MAC = …  sabit kanal = 1` ("fixed channel").

✅ Check: MAC printed matches `NOW_BED_MAC`, channel matches `NOW_CHANNEL`. Relay stays off after boot.

## Stage 3: Desk node (hub)

1. **Wire the hub** like the bed node (`5V`→`VCC`, `GND`→`GND`, `GPIO5`→`IN`), plus the **touch switch**: solder/attach a wire from **GPIO4** to a piece of **aluminium foil tape** stuck on your desk/case.
2. **Flash it** (do **not touch the foil** during the first 3 s after every boot, it calibrates):
   ```bash
   cd firmware/hub
   pio run -t upload --upload-port <PORT>
   ```
3. **Open the serial monitor** to check (the first 3 s are the touch calibration):
   ```bash
   pio device monitor -b 115200 -p <PORT>
   ```
   ✅ It prints `MASA DUGUMU  MAC = …  sabit kanal = 1` (the MAC must match `NOW_HUB_MAC`). The hub never joins a WiFi network and there is no Apple Home.
4. **Test (once the panel is ready in stage 4):** switch it from the panel → the relay clicks. Touch the foil → the relay toggles and the panel's screen follows.

   If the foil does nothing or is too sensitive: set `TOUCH_DEBUG` to `1` in `firmware/hub/src/main.cpp`, re-flash, watch `deger=` / `baseline=` in the monitor (touch and release), and adjust `TOUCH_DELTA_PCT`. *On ESP32-S3 the value goes **up** when touched.* (This S3 hub build is not hardware-tested by the author, see the README status table.)

## Stage 4: Panel (CYD)

1. Connect the CYD board over USB (CH340 boards upload at 115200 baud, already set in `platformio.ini`).
2. `NOW_CONTROLLER_MAC` in `now_config.h` must be this board's MAC address (stage 1). If it changed, re-flash the hub and bed node with that file too.
3. Flash:
   ```bash
   cd firmware/controller-cyd
   pio run -e cyd -t upload --upload-port <PORT_OF_CYD>
   ```
   The on-screen language is Turkish. For the **English UI** use `-e cyd-en`: `pio run -e cyd-en -t upload --upload-port <PORT_OF_CYD>`.
4. On first boot touch the four corner targets in turn (touch calibration). You can repeat it later in Settings → STATUS.
5. ✅ Check: the home screen shows the lamp tiles. In the serial log you should see `[now] kumanda MAC = … sabit kanal = 1` (**the same as `NOW_CHANNEL`**). While the hub and bed node are running, the "unreachable" warning disappears after a few seconds.
6. Tap the **Desk** tile: the hub's relay clicks and the tile colour changes. Tap **Bed**: the bed node's relay clicks.

## Stage 5: End-to-end test (still no mains)

- [ ] Panel → **Desk** → hub relay clicks, colours correct.
- [ ] Touch foil → relay toggles, the panel's screen follows.
- [ ] Panel → **Bed** → bed relay clicks.
- [ ] Panel → **All lights** → both relays react.
- [ ] On the panel open Settings → **LAYOUT**, change the layout/tile order; it stays after a reboot.
- [ ] Open Settings → **STATUS**: hub and bed show a dBm value (better than about −65 dBm).
- [ ] Unplug the hub: the panel shows the "Desk unreachable" warning and reverts a tapped tile after ~2 s.

If something fails, see [troubleshooting.md](troubleshooting.md).

## Stage 6: Connect the lamps (mains)

Only now. **Read [hardware.md → Mains wiring](hardware.md#mains-wiring) completely first.** In short: breaker off, only the **phase wire** goes through the relay (`COM` in, `NO` out), neutral bypasses the relay, everything in a closed insulated box, and test with the lamp still unplugged.

After wiring, repeat Stage 5's relay tests with the lamp: **lamp off when the panel says off, and lamp off after you unplug and re-plug the ESP** (power cut). If the lamp is inverted, it is wired to `NC`: move it to `NO`.

🎉 Done.
