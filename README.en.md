# Masa Kumanda (Desk Remote)

A **battery-powered touch AMOLED remote** that switches a desk lamp, a bedside lamp and (later) an LED strip over **ESP-NOW**, with no internet or router needed. The desk lamp is also exposed to **Apple Home** (HomeKit).

> Turkish is the primary language of the docs: [README.md](README.md), [docs/](docs).

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

## Highlights

- **Remote UI (LVGL 8.3):** swipe menu (Desk, Bed, LED, All, Status) with user-reorderable pages, auto portrait/landscape via **gyro**, pull-down panel with an **orientation lock**, dim/off and **deep sleep with touch wake**, charging icon, status page (RSSI in dBm, battery, channel).
- **Hub:** [HomeSpan](https://github.com/HomeSpan/HomeSpan) light accessory, capacitive-touch switch, relay, ESP-NOW ([SpanPoint](https://github.com/HomeSpan/HomeSpan/blob/master/docs/NOW.md)) link to the remote.
- **Bed node:** ESP-NOW only (no WiFi), relay output, always boots **off**.
- Reliable delivery: absolute-value `SET` commands acknowledged by an application-level `STATE` reply, retries, on-screen revert on failure.
- Channel handling that does not depend on flaky radio ACKs: nodes read the router's channel from a Wi-Fi scan of its SSID and lock to it.

## Hardware

| Part | Notes |
|---|---|
| Waveshare ESP32-S3-Touch-AMOLED-1.64 (**V1 pinout**) | Remote. CO5300 AMOLED 280x456, FT3168 touch, QMI8658 IMU, LiPo charger |
| 2 x ESP32-S3 Super Mini | Hub and bed node |
| 2 x 1-channel 5 V relay module (active-low) | Switches the lamp phase wire |
| Protected LiPo cell (with PCM) | The Waveshare board has **no** battery protection circuit |
| Aluminium foil tape | Touch switch on the desk |

## Quick start

1. Copy `firmware/shared/now_config.example.h` to `now_config.h` and fill in the three MAC addresses, your Wi-Fi SSID and an ESP-NOW passphrase (git-ignored).
2. `pio run -t upload` inside `firmware/controller`, `firmware/hub` and `firmware/bed-node`.
3. Enter the hub's Wi-Fi with the `W` command of the HomeSpan serial CLI, then pair it in Apple Home (default HomeSpan setup code `466-37-726`).

Details: [docs/setup.md](docs/setup.md) (Turkish).

## Status

Remote and bed node: tested on hardware. **Hub for ESP32-S3 Super Mini: compiles, not hardware-tested** (the author's hub is a classic ESP32 running the same logic; touch readings *rise* on S3 and *fall* on the classic ESP32, see the docs). LED strip node: planned.

## ⚠️ Safety

The relay switches **mains voltage**. Only wire it if you know what you are doing, switch the breaker off, keep all mains connections in a closed insulated enclosure. Use a **protected** LiPo cell. No warranty; use at your own risk.

## License

MIT for this repository's own code. `esp_lcd_sh8601.*` is Apache-2.0 (Espressif); display/touch init code is adapted from Waveshare's demo. Uses LVGL and HomeSpan (both MIT).
