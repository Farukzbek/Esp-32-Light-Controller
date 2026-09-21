# Third-party notices

Files in this repository that are **not** covered by the MIT license of this project:

| File | Origin | License |
|---|---|---|
| `firmware/controller/src/esp_lcd_sh8601.c`, `firmware/controller/include/esp_lcd_sh8601.h` | Espressif Systems (`esp_lcd_sh8601` component) | Apache-2.0 (see the SPDX header in each file) |
| `firmware/controller/src/lcd_bsp.c`, `firmware/controller/src/FT3168.cpp`, display/touch initialisation sequences | Adapted from [Waveshare's ESP32-S3-Touch-AMOLED-1.64 demo](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.64) | See Waveshare's demo package |
| `firmware/controller/include/lv_conf.h` | Derived from LVGL's `lv_conf_template.h` | MIT (LVGL) |

Libraries downloaded at build time by PlatformIO (not vendored): [LVGL](https://lvgl.io) (MIT), [HomeSpan](https://github.com/HomeSpan/HomeSpan) (MIT), [pioarduino platform-espressif32](https://github.com/pioarduino/platform-espressif32) / Arduino-ESP32 (LGPL-2.1 / Apache-2.0 components).
