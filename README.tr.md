# ESP32 Light Controller (Masa Kumanda)

ESP-NOW ile çalışan, **pilli ve dokunmatik AMOLED ışık kumandası**. Masa ve yatak lambasını, ileride LED şeridi, internet olmadan da kontrol eder. Router, modem ya da WiFi gerekmez; cihazlar WiFi ağ listesinde de görünmez.

> 🇬🇧 English (main README): [README.md](README.md)

```
   +----------------+   ESP-NOW   +------------------------+
   |  Kumanda       |<----------->|  Masa düğümü (hub)     |--- role --- masa lambasi
   |  Waveshare     |             |  ESP32-S3 Super Mini   |--- alüminyum bant (dokunma)
   |  AMOLED 1.64"  |   ESP-NOW   +------------------------+
   |  (pilli)       |<----------->+------------------------+
   +----------------+             |  Yatak düğümü          |--- role --- yatak lambasi
                                  |  ESP32-S3 Super Mini   |
                                  +------------------------+
        Üç cihaz sabit bir ESP-NOW kanalında buluşur (NOW_CHANNEL)
```

## Özellikler

**Kumanda (Waveshare ESP32-S3-Touch-AMOLED-1.64)**
- Kaydırmalı menü: Masa, Yatak, LED, Tüm ışıklar, Durum. Sayfaların **sırası kumandadan değiştirilir** (ayar sayfası).
- Her lambada büyük açma/kapama düğmesi, altta yeşil/kırmızı durum şeridi.
- **Dikey/yatay otomatik döner** (jiroskopla), menü de buna göre dikey/yatay kayar. Üst kenardan aşağı kaydırınca **yön kilidi** paneli iner. Cihaz sırt üstü düz yatarken yön kilitlenir.
- Ekran 15 sn'de kısılır, 60 sn'de kapanır. **Pilde** 90 sn sonra **derin uykuya** girer, dokunmayla uyanır.
- Sağ üstte şarj simgesi, hub/yatak bağlantısı kopunca uyarı.
- **Durum sayfası:** hub ve yatak sinyal gücü (dBm), pil/şarj, ESP-NOW kanalı, yön kilidi.

**Masa düğümü / hub (ESP32-S3 Super Mini)** — kapasitif dokunma bandı ile açma/kapama, röle, kumanda ile ESP-NOW. WiFi/Apple Home yok.

**Yatak düğümü (ESP32-S3 Super Mini)** — Sadece ESP-NOW, WiFi/modem yok. Elektrik gelince lamba her zaman kapalı başlar.

## Depo yapısı

| Klasör | İçerik |
|---|---|
| [`firmware/controller`](firmware/controller) | Kumanda yazılımı (PlatformIO, LVGL 8.3) |
| [`firmware/hub`](firmware/hub) | Masa düğümü / hub yazılımı (ESP32-S3 Super Mini) |
| [`firmware/bed-node`](firmware/bed-node) | Yatak düğümü yazılımı |
| [`firmware/shared`](firmware/shared) | Ortak ESP-NOW protokolü ve kişisel ayar şablonu |
| [`docs`](docs) | Mimari, donanım, protokol, kurulum, sorun giderme |

## Hızlı başlangıç

> **Her aşamada kontrol içeren tam rehber: [docs/setup.md](docs/setup.md).** 220V bağlantısı en sonda yapılır.

1. [PlatformIO](https://platformio.org/) kur (`pip install -U platformio`), depoyu indir.
2. Üç kartın MAC adresini oku (`esptool --port <PORT> read-mac`), `firmware/shared/now_config.example.h` dosyasını **`now_config.h`** olarak kopyala (git'e girmez) ve MAC'leri, ESP-NOW kanalını (`NOW_CHANNEL`, varsayılan 1) ve bir ESP-NOW parolasını doldur.
3. Waveshare kartının V1 olduğunu `firmware/tools/board-check` ile, rölenin mantığını `firmware/tools/relay-test` ile doğrula.
4. `firmware/controller`, `firmware/hub`, `firmware/bed-node` klasörlerinin her birinde `pio run -t upload --upload-port <PORT>`.

## Dokümanlar

- [Mimari ve tasarım kararları](docs/architecture.md)
- [Donanım, pinler, kablolama, 220V güvenliği](docs/hardware.md)
- Yardımcı araçlar: [`firmware/tools/board-check`](firmware/tools/board-check) (Waveshare V1 mi?), [`firmware/tools/relay-test`](firmware/tools/relay-test) (röle mantığı, 220V gerekmez)
- 🇬🇧 English guides: [docs/en](docs/en)
- [Waveshare AMOLED 1.64" notları (V1 pinleri, ekran, IMU, şarj devresi)](docs/waveshare-amoled-notes.md)
- [ESP-NOW protokolü](docs/protocol.md)
- [Kurulum](docs/setup.md)
- [Sorun giderme ve öğrenilen dersler](docs/troubleshooting.md)

## Durum

| Bileşen | Durum |
|---|---|
| Kumanda | Gerçek donanımda test edildi (V1 kart) |
| Yatak düğümü | Gerçek donanımda test edildi (ESP32-S3 Super Mini + 5V röle) |
| Hub (ESP32-S3 Super Mini) | **Derleniyor, donanımda test EDİLMEDİ.** Yazarın kendi hub'ı klasik bir ESP32 (aynı mantık, aynı kod tabanı). Dokunma eşiği S3'te farklı davranır, bkz. [docs/hardware.md](docs/hardware.md#hub-dokunma-bandı) |
| LED şerit düğümü | Planlandı, yazılmadı (protokolde yeri hazır) |

## ⚠️ Güvenlik

Röle, **220V şebeke gerilimini** anahtarlar. Bağlantıyı yalnızca ne yaptığını biliyorsan yap, **sigortayı kapat**, tüm yüksek gerilim bağlantılarını kapalı ve yalıtkan bir kutuda tut, emin değilsen bir elektrikçiye yaptır. Ayrıntılar: [docs/hardware.md](docs/hardware.md#220v-tarafı-güvenlik). Bu depo garanti vermez, kullanım sorumluluğu sana aittir.

Pil için **korumalı (PCM'li)** LiPo kullan: Waveshare kartında aşırı deşarj/kısa devre koruması yoktur.

## Lisans ve atıflar

Üçüncü taraf dosyalar için: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

- Bu deponun kendi kodu: [MIT](LICENSE).
- `firmware/controller/src/esp_lcd_sh8601.c` ve `include/esp_lcd_sh8601.h`: Espressif Systems, Apache-2.0 (dosya başlıklarında belirtilmiştir).
- Ekran/dokunmatik başlatma kodu ve `lcd_bsp.c` / `FT3168.cpp`: [Waveshare'in ESP32-S3-Touch-AMOLED-1.64 örnek kodundan](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.64) uyarlanmıştır.
- Kullanılan kütüphaneler: [LVGL](https://lvgl.io) (MIT), [HomeSpan](https://github.com/HomeSpan/HomeSpan) (MIT, yalnızca ESP-NOW/`SpanPoint` sınıfı için).
