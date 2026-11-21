# Adım adım kurulum rehberi

> 🇬🇧 English version: [en/setup.md](en/setup.md)

Aşamaları **sırayla** takip et. Her aşama, çalıştığından emin olman için bir "✅ Kontrol" ile biter. **220V şebeke gerilimi en son** bağlanır, ondan önce her şey masa üstünde (5V ile) çalışıyor olmalı.

Tahmini süre: 1–2 saat (ilk derleme araç zincirini indirdiği için birkaç dakika sürer).

| Aşama | Ne | Gerekenler |
|---|---|---|
| 0 | Araçları kur, projeyi indir | Bilgisayar |
| 1 | MAC adreslerini oku, `now_config.h` oluştur | 3 kart |
| 2 | Yatak düğümü masa üstünde (röle tık testi) | S3 Super Mini + röle |
| 3 | Masa düğümü (hub): dokunma bandı + röle | S3 Super Mini + röle + folyo bant |
| 4 | Kumanda paneli (CYD) | CYD kartı |
| 5 | Uçtan uca test (hala 220V yok) | hepsi |
| 6 | Lambaları bağla (220V) | [hardware.md](hardware.md#220v-tarafı-güvenlik), önce güvenlik bölümünü oku |

---

## Aşama 0: Araçlar

1. **PlatformIO Core** kur (Python 3.8+):
   ```bash
   pip install -U platformio        # her işletim sistemi
   # macOS alternatifi:  brew install platformio
   # ya da VS Code "PlatformIO IDE" eklentisi
   ```
   ✅ `pio --version` bir sürüm yazmalı.
2. **Projeyi indir:**
   ```bash
   git clone https://github.com/Farukzbek/Nowlight.git
   cd Nowlight
   ```
3. **Kartın seri portunu bul** (bir kartı tak):
   | İşletim sistemi | Nasıl | Örnek |
   |---|---|---|
   | macOS | `ls /dev/cu.usbmodem*` | `/dev/cu.usbmodem1101` |
   | Linux | `ls /dev/ttyACM*` (bir kere `sudo usermod -aG dialout $USER`, sonra oturumu kapat-aç) | `/dev/ttyACM0` |
   | Windows | Aygıt Yöneticisi → Bağlantı Noktaları (COM ve LPT) | `COM3` |

   ESP32-S3 kartlar çipin **yerel USB**'sini kullanır, normalde ayrı sürücü gerekmez.

## Aşama 1: MAC adresleri ve `now_config.h`

ESP-NOW cihazları **MAC adresiyle** eşler, bu yüzden yüklemeden önce üçünü de bilmen gerekir. MAC'i okumak firmware olmayan yeni bir kartta bile çalışır:

```bash
pip install esptool
esptool --port <PORT> read-mac
```

Kartları **tek tek** tak, hangisinin hangi MAC olduğunu not et:

| Kart | Rol | MAC (örnek) |
|---|---|---|
| CYD (ESP32-2432S028R) | Kumanda paneli (`NOW_CONTROLLER_MAC`) | `A0:B1:C2:D3:E4:F5` |
| S3 Super Mini #1 | Hub (`NOW_HUB_MAC`) | … |
| S3 Super Mini #2 | Yatak düğümü (`NOW_BED_MAC`) | … |

Sonra kendi ayar dosyanı oluştur (`.gitignore`'da, bilgisayarından çıkmaz):

```bash
cp firmware/shared/now_config.example.h firmware/shared/now_config.h
```

`now_config.h` içinde şunları doldur:
- üç MAC adresi (**BÜYÜK harf**, iki nokta ile ayrılmış),
- `NOW_CHANNEL`: üç cihazın buluşacağı ESP-NOW kanalı (1–13, varsayılan 1). WiFi/router gerekmez. Komşu/router'ın 2,4 GHz kanalıyla çakışmamasında fayda var,
- `NOW_PASSWORD`: üç cihazda **aynı** olacak bir parola. Varsayılanı değiştir.

✅ Bu dosya yokken derlersen `#error "now_config.h yok…"` mesajı görürsün. Dosya varken `pio run` çalışır.

## Aşama 2: Yatak düğümü masa üstünde

1. Bir röle modülünün **sadece düşük gerilim tarafını** bir ESP32-S3 Super Mini'ye bağla:

   | S3 Super Mini | Röle modülü |
   |---|---|
   | `5V` | `VCC` |
   | `GND` | `GND` |
   | `5` (GPIO5) | `IN` |

   **Rölenin vidalı klemenslerine henüz hiçbir şey bağlama.**
2. **Röle mantığı testi:**
   ```bash
   cd firmware/tools/relay-test
   pio run -t upload --upload-port <PORT>
   pio device monitor -b 115200 -p <PORT>
   ```
   Monitör 2 saniyede bir `GPIO5 = LOW` / `GPIO5 = HIGH` yazar. Yaygın **ters mantıklı (active-low)** modülde röle **LOW yazarken tık yapar** (modül LED'i yanar). HIGH iken tık yapıyorsa modülün normal mantıklıdır: `firmware/hub/src/main.cpp`'de `RELAY_ACTIVE_LOW`'u `0` yap, `firmware/bed-node/src/main.cpp`'de `RELAY_ON`/`RELAY_OFF` değerlerini ters çevir.
3. **Yatak düğümünü yükle:**
   ```bash
   cd ../../bed-node
   pio run -t upload --upload-port <PORT>
   pio device monitor -b 115200 -p <PORT>
   ```
   Röle **kapalı** açılır ve `YATAK DUGUMU  MAC = …  sabit kanal = 1` yazar.

✅ Yazdığı MAC `NOW_BED_MAC` ile, kanal `NOW_CHANNEL` ile aynı. Röle açılışta kapalı kalıyor.

## Aşama 3: Masa düğümü (hub)

1. **Hub'ı bağla:** yatak düğümü gibi (`5V`→`VCC`, `GND`→`GND`, `GPIO5`→`IN`) ve ek olarak **dokunma bandı**: **GPIO4**'ten bir kabloyu masana/kasana yapıştırdığın **alüminyum folyo banda** bağla.
2. **Yükle** (her açılışta ilk **3 saniye folyoya dokunma**, kalibrasyon yapıyor):
   ```bash
   cd firmware/hub
   pio run -t upload --upload-port <PORT>
   ```
3. **Kontrol için seri monitörü aç** (başlangıçta 3 sn dokunma kalibrasyonu var):
   ```bash
   pio device monitor -b 115200 -p <PORT>
   ```
   ✅ `MASA DUGUMU  MAC = …  sabit kanal = 1` yazar (MAC `NOW_HUB_MAC` ile aynı olmalı). Hub WiFi'ye bağlanmaz, Apple Home yoktur.
4. **Test (kumanda paneli Aşama 4'te hazır olunca):** panelden aç/kapa → röle tıklar. Folyoya dokun → röle değişir, panelin ekranı takip eder.

   Folyo tepki vermiyor ya da çok hassassa: `firmware/hub/src/main.cpp` içinde `TOUCH_DEBUG`'ı `1` yap, yeniden yükle, monitörde `deger=` / `baseline=` değerlerine dokunarak/bırakarak bak ve `TOUCH_DELTA_PCT`'yi ayarla. *ESP32-S3'te dokununca değer **yükselir**.* (Bu S3 hub sürümü yazar tarafından donanımda test edilmedi, README'deki durum tablosuna bak.)

## Aşama 4: Kumanda paneli (CYD)

1. CYD kartını USB ile bağla (CH340'lı kartlarda yükleme 115200 baud'dur, `platformio.ini`'de ayarlı).
2. `now_config.h` içindeki `NOW_CONTROLLER_MAC` bu kartın MAC adresi olmalı (Aşama 1). Hub ve yatak düğümünü bu dosyayla (MAC değiştiyse) yeniden yüklemeyi unutma.
3. Yükle:
   ```bash
   cd firmware/controller-cyd
   pio run -e cyd -t upload --upload-port <CYD_PORTU>
   ```
   Ekran dili Türkçe. **İngilizce arayüz** için `-e cyd-en` kullan: `pio run -e cyd-en -t upload --upload-port <CYD_PORTU>`.
4. İlk açılışta ekranda dört köşedeki hedeflere sırayla dokun (dokunmatik kalibrasyonu). Sonradan Ayarlar → DURUM'dan tekrarlanabilir.
5. ✅ **Kontrol:** ana ekranda lamba karoları görünür. Seri log'da `[now] kumanda MAC = … sabit kanal = 1` (**`NOW_CHANNEL` ile aynı**). Hub ve yatak düğümü çalışırken "ulaşılamıyor" uyarısı birkaç saniye içinde kaybolur.
6. **MASA** karosuna dokun: hub'ın rölesi tıklar, karo rengi değişir. **YATAK**'a dokun: yatak düğümünün rölesi tıklar.

## Aşama 5: Uçtan uca test (hala 220V yok)

- [ ] Panel → **MASA** → hub rölesi tıklıyor, renkler doğru.
- [ ] Folyoya dokun → röle değişiyor, panelin ekranı takip ediyor.
- [ ] Panel → **YATAK** → yatak rölesi tıklıyor.
- [ ] Panel → **Tüm ışıklar** → iki röle de tepki veriyor.
- [ ] Panelde Ayarlar → **DÜZEN**'den düzeni/karo sırasını değiştir, yeniden başlatınca kalıyor.
- [ ] Ayarlar → **DURUM**: hub ve yatak için dBm görünüyor (yaklaşık −65 dBm'den iyi olmalı).
- [ ] Hub'ın fişini çek: panel "Masa ulaşılamıyor" uyarısını gösteriyor ve basılan karo ~2 sn sonra eski haline dönüyor.

Bir şey olmazsa [troubleshooting.md](troubleshooting.md).

## Aşama 6: Lambaları bağla (220V)

Ancak şimdi. **Önce [hardware.md → 220V tarafı](hardware.md#220v-tarafı-güvenlik) bölümünü tamamen oku.** Özet: sigorta kapalı, röleden **sadece faz** geçer (`COM` giriş, `NO` çıkış), nötr röleyi atlar, hepsi kapalı yalıtkan kutuda, ilk denemede lamba prizde değil.

Bağladıktan sonra Aşama 5'teki röle testlerini lambayla tekrarla: **panel "kapalı" derken lamba sönük olmalı** ve **ESP'nin fişini çekip taktığında (elektrik kesintisi gibi) lamba kapalı başlamalı.** Lamba ters çalışıyorsa `NC`'ye bağlıdır: `NO`'ya taşı.

🎉 Bitti.
