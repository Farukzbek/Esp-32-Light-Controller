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

## Hub / Apple Home

| Symptom | Cause | Fix |
|---|---|---|
| Hub tries to join a network named `␛[B␛[B1…` | Arrow keys were pressed inside the serial monitor during `W` | Run `W` again; type only the network **number** or exact name, no arrows/spaces |
| `Unknown command: ' W'` | A space or stray character was sent before `W` | Press Enter once, then type only `W` and Enter |
| Apple Home cannot find "Masa Lambasi" | iPhone not on the same WiFi, router client isolation, no local-network permission | Same network, disable AP isolation, allow *Local Network* for the Home app |
| Pairing lost after re-flashing | NVS or partition table changed, or accessory structure changed | Keep `huge_app.csv`, do not erase flash, do not change accessory/service/characteristic order |
| Home response jitters by 100+ ms | WiFi power save | Already disabled in the hub firmware (`WIFI_PS_NONE`) |
| Touch switch does nothing / too sensitive | Threshold depends on foil size and wire length; S3 readings **rise** | `TOUCH_DEBUG=1`, tune `TOUCH_DELTA_PCT` |
| Wrong lamp state after touching during boot | Calibration ran while touched | Reboot and keep hands off for the first 3 s |

## ESP-NOW between the devices

| Symptom | Cause | Fix |
|---|---|---|
| Remote shows a warning triangle with `HUB` or `YATAK` | Device not reachable / different channel | Check the device is powered; compare the channel in the logs (`kanal`), fix your router's 2.4 GHz channel; make sure `NOW_ROUTER_SSID` is the network the **hub** joins |
| Everything worked, then stopped after a router reboot | Router picked another channel | Set a fixed channel; nodes re-read the channel by scanning the SSID (may take up to a minute) |
| A tap reverts after ~2 s | The command was not answered (device off, other channel, weak signal) | See above; open the **Status** page and check dBm (better than −65 good, worse than −78 marginal) |
| Command sometimes takes ~1 s | Radio packet loss, retried | Move the node away from metal, relays and mains wires; use a good 5 V adapter; keep the antenna end of the board free |
| Lamp state on the screen is inverted | Lamp wired to relay `NC` | Move it to `NO` ([hardware](hardware.md#mains-wiring)) |
| Devices don't see each other at all | Different `NOW_PASSWORD`, wrong MAC in config | The three devices need the same passphrase; re-check the MAC addresses (`esptool read-mac`) |

## Remote (Waveshare)

| Symptom | Cause | Fix |
|---|---|---|
| Blank/garbled screen | Not a V1 board (different pins) | Run `firmware/tools/board-check` |
| Screen updates only partly | Odd draw-buffer height | Keep the buffer height **even** (`lcd_config.h`) |
| Auto-rotation never works or is inverted | Faulty/unusual IMU axis | The firmware uses the gyro; if inverted change `ORIENT_GYRO_SIGN` in `app_ui.cpp` |
| Won't rotate while held tilted back | "Lying flat" lock | It locks only when almost flat for 1 s; use the pull-down orientation lock panel if you want it fixed |
| Doesn't wake from sleep with a quick tap | Wake-up polls every 300 ms | Press the screen for ~0.5 s |
| Never sleeps while plugged in | By design (external power) | Sleep only happens on battery |
| Battery drains fast | Radio is on while awake; deep sleep only after 90 s idle | Put it on the dock when unused; lower `IDLE_SLEEP_MS` |
