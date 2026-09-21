# ESP32 Light Controller

**🇬🇧** A **battery-powered touch AMOLED remote** that switches lamps (and later an LED strip) over **ESP-NOW**: no internet, no router needed. One lamp is also exposed to **Apple Home** (HomeKit) through an ESP32-S3 hub.
**🇹🇷** ESP-NOW ile **internetsiz çalışan, pilli, dokunmatik AMOLED ışık kumandası**. Bir lamba ayrıca ESP32-S3 hub üzerinden **Apple Home**'a bağlıdır.

> 🇹🇷 Türkçe README: [README.tr.md](README.tr.md). The install guide, hardware and troubleshooting docs are available in English ([`docs/en`](docs/en)); the design notes and code comments are in Turkish.

```
                    Apple Home (iPhone)
                           |  WiFi (HomeKit)
                           v
 +--------------+  ESP-NOW  +---------------------+
 |  Remote      |<--------->|  Hub (ESP32-S3)     |-- relay -- desk lamp
 |  Waveshare   |           |  HomeSpan + touch   |-- foil tape (touch switch)
 |  AMOLED 1.64"|  ESP-NOW  +---------------------+
 |  (battery)   |<--------->+---------------------+
 +--------------+           |  Bed node (ESP32-S3)|-- relay -- bedside lamp
                            +---------------------+
```

## Features

**Remote (Waveshare ESP32-S3-Touch-AMOLED-1.64, LVGL 8.3)**
- Swipe menu: Desk, Bed, LED, All lights, Status. **Page order is user-editable** on the device (settings page), saved to flash.
- Big on/off button per lamp with a green/red status bar.
- **Auto portrait/landscape** using the gyro (the menu scrolls vertically in portrait, horizontally in landscape). Pull down from the top edge for a panel with an **orientation lock**. Rotation also locks while the device lies flat on its back.
- Dim after 15 s, off after 60 s, **deep sleep after 90 s on battery** with touch wake-up. Charging icon. Warning when the hub or bed node is unreachable.
- **Status page:** RSSI (dBm) of the hub and bed node, battery/charging, ESP-NOW channel, lock state.

**Hub (ESP32-S3 Super Mini)** ([HomeSpan](https://github.com/HomeSpan/HomeSpan)): Apple Home light, capacitive-touch switch (aluminium foil tape), relay, ESP-NOW link to the remote.

**Bed node (ESP32-S3 Super Mini):** ESP-NOW only (no WiFi at all), relay, always boots **off**.

**Reliability:** absolute-value commands acknowledged by an application-level reply (not the radio ACK), retries, on-screen rollback on failure; channel discovery by scanning the router's SSID and locking the radio to that channel (SpanPoint's own channel hopping is avoided, see [architecture](docs/architecture.md), Turkish).

## Hardware

| Qty | Part | Notes |
|---|---|---|
| 1 | Waveshare **ESP32-S3-Touch-AMOLED-1.64** | Remote. This repo targets the **V1 pinout** (V2 differs, see [notes](docs/waveshare-amoled-notes.md), Turkish) |
| 2 | ESP32-S3 Super Mini | Hub and bed node |
| 2 | 1-channel 5 V relay module (**active-low**) | Switches the lamp's phase wire |
| 1 | LiPo cell **with protection circuit (PCM)** | The Waveshare board has **no** protection circuit |
| 1 | Aluminium foil tape | Touch switch on the hub |
| 2 | 5 V USB adapters (>=1 A) | Hub and bed node (they also power the relay) |

Pins, wiring and mains safety: [docs/en/hardware.md](docs/en/hardware.md).

## Quick start

> **Full step-by-step guide with a check after every stage: [docs/en/setup.md](docs/en/setup.md).** Mains wiring comes last.

1. Install [PlatformIO](https://platformio.org/) (`pip install -U platformio`) and clone this repo.
2. Read the three boards' MAC addresses (`esptool --port <PORT> read-mac`), copy `firmware/shared/now_config.example.h` to **`now_config.h`** (git-ignored) and fill in the MACs, the 2.4 GHz WiFi SSID the **hub** joins, and an ESP-NOW passphrase.
3. Check your Waveshare board is a V1 with `firmware/tools/board-check`, test the relay with `firmware/tools/relay-test`.
4. In each of `firmware/controller`, `firmware/hub`, `firmware/bed-node`: `pio run -t upload --upload-port <PORT>`.
5. Give the hub its WiFi with the `W` command of the HomeSpan serial CLI, add it in Apple Home (default code `466-37-726`).
6. Fix your router's 2.4 GHz channel (1, 6 or 11).

## Documentation

**English** ([`docs/en`](docs/en)):

| Doc | Content |
|---|---|
| [setup.md](docs/en/setup.md) | **Step-by-step install guide** (8 stages with checks, mains last) |
| [hardware.md](docs/en/hardware.md) | BOM, pins, relay wiring, terminal identification, **mains safety**, power |
| [troubleshooting.md](docs/en/troubleshooting.md) | Install, Apple Home, ESP-NOW and remote problems with fixes |

**Turkish** ([`docs`](docs)): the same guides plus deeper design notes:

| Doc | Content |
|---|---|
| [architecture.md](docs/architecture.md) | Roles, data flow, reliability, channel handling, sleep strategy |
| [protocol.md](docs/protocol.md) | 9-byte `NowMsg`, flows, how to add a node (e.g. LED strip) |
| [waveshare-amoled-notes.md](docs/waveshare-amoled-notes.md) | V1 pins, CO5300 display quirks, touch, IMU fault, charger/battery path from the schematic |

Helper tools: [`firmware/tools/board-check`](firmware/tools/board-check) (is my Waveshare a V1?) and [`firmware/tools/relay-test`](firmware/tools/relay-test) (relay polarity, no mains needed).

## Status

| Component | Status |
|---|---|
| Remote | Tested on real hardware (V1 board) |
| Bed node | Tested on real hardware (ESP32-S3 Super Mini + 5 V relay) |
| Hub for ESP32-S3 Super Mini | **Compiles, NOT hardware-tested.** The author's hub is a classic ESP32 running the same logic. Touch readings *rise* on S3 and *fall* on the classic ESP32; see [docs/en/hardware.md](docs/en/hardware.md#hub-touch-switch) for tuning (`TOUCH_DEBUG`, `TOUCH_DELTA_PCT`) |
| LED strip node | Planned (the protocol already reserves a device slot) |

Known limitations: the on-screen font is ASCII only; the battery percentage is an estimate (no fuel gauge); the author's board has a faulty accelerometer axis, so orientation uses the gyro; ESP-NOW between the remote and the bed node shows ~10 % radio-ACK loss in the author's setup (commands are retried and idempotent).

## ⚠️ Safety

The relay switches **mains voltage**. Only wire it if you know what you are doing, switch the breaker off first, keep every mains connection in a closed insulated enclosure, and ask an electrician if unsure. Use a **protected** LiPo cell. No warranty; use at your own risk.

## Contributing

Issues and pull requests are welcome, especially: a tested ESP32-S3 hub configuration, the Waveshare **V2** pinout, an LED-strip node, and English translations of the design notes (`architecture.md`, `protocol.md`, `waveshare-amoled-notes.md`).

## License and credits

- This repository's own code: [MIT](LICENSE).
- Third-party files (details in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)):
  - `firmware/controller/src/esp_lcd_sh8601.c` and `include/esp_lcd_sh8601.h`: Espressif Systems, Apache-2.0 (see file headers).
  - Display/touch initialisation and `lcd_bsp.c` / `FT3168.cpp`: adapted from [Waveshare's ESP32-S3-Touch-AMOLED-1.64 demo](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.64).
- Libraries (downloaded at build time): [LVGL](https://lvgl.io) (MIT), [HomeSpan](https://github.com/HomeSpan/HomeSpan) (MIT).
