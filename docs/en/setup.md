# Step-by-step setup guide

Follow the stages **in order**. Every stage ends with a "✅ Check" so you know it worked before you move on. Mains voltage (220/230 V) is connected **last**, after everything works on the bench.

Estimated time: 1–2 hours (the first build downloads toolchains and takes a few minutes).

| Stage | What | Needs |
|---|---|---|
| 0 | Install tools, download the project | Computer |
| 1 | Read the boards' MAC addresses, create `now_config.h` | 3 boards |
| 2 | Check your Waveshare board is **V1** | Waveshare board |
| 3 | Bed node on the bench (relay click test) | S3 Super Mini + relay |
| 4 | Desk node (hub): touch switch + relay | S3 Super Mini + relay + foil tape |
| 5 | Remote (Waveshare) | Waveshare + LiPo |
| 6 | End-to-end test (still no mains) | everything |
| 7 | Connect the lamps (mains) | see [hardware.md](hardware.md#mains-wiring) — read the safety section first |

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
   git clone https://github.com/Farukzbek/Esp-32-Light-Controller.git
   cd Esp-32-Light-Controller
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
| Waveshare AMOLED | Remote (`NOW_CONTROLLER_MAC`) | `A0:B1:C2:D3:E4:F5` |
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

## Stage 2: Is your Waveshare board a V1?

Waveshare sold two hardware revisions with different pins. This project uses the **V1** pins. Test yours (this takes one minute):

```bash
cd firmware/tools/board-check
pio run -t upload --upload-port <PORT_OF_WAVESHARE>
pio device monitor -b 115200 -p <PORT_OF_WAVESHARE>
```

Expected output for a V1 board:
```
  cihaz bulundu: 0x38  <- FT3168 dokunmatik
  cihaz bulundu: 0x6B  <- QMI8658 IMU
SONUC: OK. Kartin V1 pinout'una uyuyor ...
```
(The messages are Turkish; `OK` / `V1` is what matters.) If it says the touch controller was **not found**, your board is probably a V2, you will have to change the pins in `firmware/controller/include/lcd_config.h` using Waveshare's V2 demo ([notes](../waveshare-amoled-notes.md)).

If the upload fails, hold the **BOOT** button while plugging the USB cable in, then try again. Leave the monitor with `Ctrl+C`.

✅ Check: `SONUC: OK`.

## Stage 3: Bed node on the bench

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

## Stage 4: Desk node (hub)

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
4. **Test (once the remote is ready in stage 5):** switch it from the remote → the relay clicks. Touch the foil → the relay toggles and the remote screen follows.

   If the foil does nothing or is too sensitive: set `TOUCH_DEBUG` to `1` in `firmware/hub/src/main.cpp`, re-flash, watch `deger=` / `baseline=` in the monitor (touch and release), and adjust `TOUCH_DELTA_PCT`. *On ESP32-S3 the value goes **up** when touched.* (This S3 hub build is not hardware-tested by the author, see the README status table.)

## Stage 5: Remote (Waveshare)

1. Connect a **protected** LiPo cell (with protection circuit) to the MX1.25 connector. **Check the polarity first**: generic cells are often the opposite of the board's connector.
2. Flash:
   ```bash
   cd firmware/controller
   pio run -t upload --upload-port <PORT_OF_WAVESHARE>
   ```
3. ✅ Check: the screen shows the menu. In the serial log you should see `[now] kumanda MAC = … sabit kanal = 1` (**the same as `NOW_CHANNEL`**). While the hub is running, the top-left warning triangle disappears after a few seconds.
4. Swipe between pages; tap the big button on **MASA**: the hub's relay clicks and the button turns yellow/green. Tap **YATAK**: the bed node's relay clicks.

## Stage 6: End-to-end test (still no mains)

- [ ] Remote → **MASA** → hub relay clicks, colours correct.
- [ ] Touch foil → relay toggles, remote screen follows.
- [ ] Remote → **YATAK** → bed relay clicks.
- [ ] Remote → **HEPSI / ALL** → both relays react.
- [ ] Open the remote's last page (**menu order**) and move a page with the arrows; it stays after a reboot.
- [ ] Open the **Status** page: hub and bed show a dBm value (green is better than about −65 dBm).
- [ ] Unplug the hub: the remote shows the warning triangle and reverts a tapped button after ~2 s.

If something fails, see [troubleshooting.md](troubleshooting.md).

## Stage 6: Connect the lamps (mains)

Only now. **Read [hardware.md → Mains wiring](hardware.md#mains-wiring) completely first.** In short: breaker off, only the **phase wire** goes through the relay (`COM` in, `NO` out), neutral bypasses the relay, everything in a closed insulated box, and test with the lamp still unplugged.

After wiring, repeat Stage 6's relay tests with the lamp: **lamp off when the remote says off, and lamp off after you unplug and re-plug the ESP** (power cut). If the lamp is inverted, it is wired to `NC`: move it to `NO`.

🎉 Done.
