# Kapı paneli (CYD, ESP32-2432S028R)

2.8" 320x240 dokunmatik ekranlı CYD kartı, odanın girişinde sabit bir ışık paneli olarak çalışır. Aynı ESP-NOW protokolüyle (sabit kanal, WiFi/router yok) masa ve yatak düğümlerini yönetir. Sistemin kumandasıdır; istenirse birden fazla panel kullanılabilir (her panelin MAC'i ayrı tanıtılmalıdır).

- **Ana ekran:** 4'lü, 3'lü, 2'li ya da 1'li düzen; karo sırası ve gizleme ayarlanabilir.
- **Ayarlar (sağ üst dişli):** DÜZEN, EKRAN (parlaklık, otomatik parlaklık, ekranı çevir, ekran kapanma süresi), DURUM (sinyal gücü, kanal, dokunmatik kalibrasyonu).
- **Dil:** Türkçe (Türkçe harfli yazı tipi) ya da İngilizce.

## Derleme ve yükleme

`firmware/shared/now_config.h` içindeki `NOW_CONTROLLER_MAC`, bu CYD'nin MAC adresi olmalı; masa ve yatak düğümlerinde de aynı MAC yazılı olmalı.

```bash
cd firmware/controller-cyd
pio run -e cyd    -t upload --upload-port <PORT>   # Türkçe
pio run -e cyd-en -t upload --upload-port <PORT>   # İngilizce
```

CH340'lı kartlarda yükleme 115200 baud'dur (`platformio.ini`'de ayarlı). İlk açılışta dört köşedeki hedeflere dokunarak dokunmatik kalibrasyonu yapılır (Ayarlar → DURUM'dan tekrarlanabilir).

## Geliştirme aracı

`pio run -e cyd-debug -t upload` ile seriden `tap`, `drag`, `snap`, `dev`, `mem` komutları açılır; `tools/snap.py` ile ekranın PNG görüntüsü alınıp sanal dokunmayla arayüz doğrulanabilir. Normal kullanımda `cyd` ya da `cyd-en` yükle.

## Notlar

- Ekran paneli TFT_eSPI'nin `ILI9341_2_DRIVER` varyantı + ters renk; ayarlar `platformio.ini`'de.
- CYD'nin ışık sensörü (LDR) ters çalışır: ışık artınca voltaj düşer. Otomatik parlaklık bunu telafi eder.
- Yazı tipleri LVGL paketindeki Montserrat'tan (SIL OFL) `tools/gen_fonts.sh` ile üretilir.
