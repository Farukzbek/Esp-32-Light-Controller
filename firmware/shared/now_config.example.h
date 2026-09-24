#pragma once
// Bu dosyayi "now_config.h" adiyla kopyala ve KENDI degerlerinle doldur.
// now_config.h .gitignore'da: MAC adresleri, ag adi ve sifreleme anahtari depoya gitmez.
//
// Cihazlarin MAC adresini okumak icin (kart USB'ye takili iken):
//   pio pkg exec -p tool-esptoolpy -- esptool.py --port <PORT> read_mac
// ya da ilk acilis seri log'unda "MAC = ..." satirina bak.

// Wi-Fi STA MAC adresleri (buyuk harf, iki nokta ustuste ile)
#define NOW_HUB_MAC         "AA:BB:CC:00:00:01"   // masa lambasi dugumu (hub)
#define NOW_CONTROLLER_MAC  "AA:BB:CC:00:00:02"   // Waveshare AMOLED kumanda
#define NOW_BED_MAC         "AA:BB:CC:00:00:03"   // yatak ESP32-S3 Super Mini

// ESP-NOW kanali (1..13). Uc cihaz bu kanalda SABIT bulusur; WiFi/router/modem gerekmez.
// Evde ayni kanali kullanan baska bir 2.4 GHz ag varsa (router) farkli bir kanal secmek iyi olur.
#define NOW_CHANNEL         1

// ESP-NOW sifreleme anahtari uretmek icin parola. Uc cihazda AYNI olmali. Degistir!
#define NOW_PASSWORD        "change-me-please"
