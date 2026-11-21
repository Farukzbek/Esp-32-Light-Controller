# Troubleshooting

## Install / flashing

| Symptom | Cause | Fix |
|---|---|---|
| `pio: command not found` | PlatformIO not installed / not in PATH | `pip install -U platformio`, open a new terminal |
| `#error "now_config.h yok…"` when building | You did not create your config | `cp firmware/shared/now_config.example.h firmware/shared/now_config.h` and fill it in ([setup](setup.md#stage-1-mac-addresses-and-now_configh)) |
| First build is very slow / seems stuck | It downloads the ESP32 toolchain and libraries | Wait a few minutes; needs internet |
| No serial port appears | Charge-only USB cable, or port permissions | Use a data cable; Linux: `sudo usermod -aG dialout $USER` and log out/in |
| ESP32-S3 Super Mini: monitor shows `waiting for download` | Board was plugged in with **BOOT** held | Unplug and re-plug **without** pressing BOOT |
| Upload fails on an S3 board | Not in download mode | Hold **BOOT**, plug in USB, release, upload again |
| `Could not exclusively lock port` | Another monitor is holding the port | Close the other terminal (`Ctrl+C`) |
| Boards with a **CH340** USB chip fail with `Invalid head of packet` | Upload speed too high | Set `upload_speed = 115200` in `platformio.ini` |
| Serial log starts empty | Opening the port resets the board | Wait for the next messages or press the board's reset |

## Desk node (hub)

| Symptom | Cause | Fix |
|---|---|---|
| Touch switch does nothing / too sensitive | Threshold depends on foil size and wire length; S3 readings **rise** | `TOUCH_DEBUG=1`, tune `TOUCH_DELTA_PCT` |
| Wrong lamp state after touching during boot | Calibration ran while touched | Reboot and keep hands off for the first 3 s |

## ESP-NOW between the devices

| Symptom | Cause | Fix |
|---|---|---|
| Remote shows a warning triangle with `HUB` or `YATAK` | Device not reachable / different channel | Check the device is powered; every device's log must print the same `sabit kanal = N` (fixed channel) |
| Frequent packet loss near a busy router | Same 2.4 GHz channel is crowded | Change `NOW_CHANNEL` to a quiet one (1, 6, 11) and re-flash all three devices |
| A tap reverts after ~2 s | The command was not answered (device off, other channel, weak signal) | See above; open the **Status** page and check dBm (better than −65 good, worse than −78 marginal) |
| Command sometimes takes ~1 s | Radio packet loss, retried | Move the node away from metal, relays and mains wires; use a good 5 V adapter; keep the antenna end of the board free |
| Lamp state on the screen is inverted | Lamp wired to relay `NC` | Move it to `NO` ([hardware](hardware.md#mains-wiring)) |
| Devices don't see each other at all | Different `NOW_CHANNEL` / `NOW_PASSWORD`, wrong MAC in config | The three devices need the same passphrase; re-check the MAC addresses (`esptool read-mac`) |

## Panel (CYD)

| Symptom | Cause | Fix |
|---|---|---|
| Inverted colours / garbled screen | Different panel variant | Check `ILI9341_2_DRIVER` + `TFT_INVERSION_ON` in `platformio.ini` (see the CYD notes in the project README) |
| Touch lands in the wrong place | Calibration missing/bad | Settings → STATUS → "Calibrate touch" |
| Upload fails or stops half way | CH340 is unreliable at high speed | `upload_speed = 115200` (already set); change cable/port, or hold BOOT while uploading |
| Panel shows an "unreachable" warning | `NOW_CONTROLLER_MAC` is not the CYD's MAC / different channel | Put the CYD's MAC (`esptool read-mac`) into the hub's and bed node's `now_config.h`, use the same `NOW_CHANNEL`, re-flash the nodes |
| Screen turns off | "Screen off after" setting | Settings → SCREEN → "Screen off after" → Never |
