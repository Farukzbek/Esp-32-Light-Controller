# Adım adım kurulum rehberi

> 🇬🇧 English version: [en/setup.md](en/setup.md)

Aşamaları **sırayla** takip et. Her aşama, çalıştığından emin olman için bir "✅ Kontrol" ile biter. **220V şebeke gerilimi en son** bağlanır, ondan önce her şey masa üstünde (5V ile) çalışıyor olmalı.

Tahmini süre: 1–2 saat (ilk derleme araç zincirini indirdiği için birkaç dakika sürer).

| Aşama | Ne | Gerekenler |
|---|---|---|
| 0 | Araçları kur, projeyi indir | Bilgisayar |
| 1 | MAC adreslerini oku, `now_config.h` oluştur | 3 kart |
| 2 | Waveshare kartının **V1** olduğunu doğrula | Waveshare kart |
| 3 | Yatak düğümü masa üstünde (röle tık testi) | S3 Super Mini + röle |
| 4 | Masa düğümü (hub): dokunma bandı + röle | S3 Super Mini + röle + folyo bant |
| 5 | Kumanda (Waveshare) | Waveshare + LiPo |
| 6 | Uçtan uca test (hala 220V yok) | hepsi |
| 7 | Lambaları bağla (220V) | [hardware.md](hardware.md#220v-tarafı-güvenlik), önce güvenlik bölümünü oku |

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
   git clone https://github.com/Farukzbek/Esp-32-Light-Controller.git
   cd Esp-32-Light-Controller
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
| Waveshare AMOLED | Kumanda (`NOW_CONTROLLER_MAC`) | `A0:B1:C2:D3:E4:F5` |
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

## Aşama 2: Waveshare kartın V1 mi?

Waveshare iki donanım revizyonu çıkardı, pinleri farklı. Bu proje **V1** pinleri içindir. Kendi kartını bir dakikada test et:

```bash
cd firmware/tools/board-check
pio run -t upload --upload-port <WAVESHARE_PORTU>
pio device monitor -b 115200 -p <WAVESHARE_PORTU>
```

V1 kartta beklenen çıktı:
```
  cihaz bulundu: 0x38  <- FT3168 dokunmatik
  cihaz bulundu: 0x6B  <- QMI8658 IMU
SONUC: OK. Kartin V1 pinout'una uyuyor ...
```
Dokunmatik **bulunamadı** derse kartın büyük ihtimalle V2'dir: `firmware/controller/include/lcd_config.h` içindeki pinleri Waveshare'in V2 örneğine göre değiştirmen gerekir ([notlar](waveshare-amoled-notes.md)). Yükleme olmazsa USB'yi **BOOT** düğmesine basılı tutarak tak. Monitörden `Ctrl+C` ile çık.

✅ `SONUC: OK`.

## Aşama 3: Yatak düğümü masa üstünde

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

## Aşama 4: Masa düğümü (hub)

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
4. **Test (kumanda Aşama 5'te hazır olunca):** kumandadan aç/kapa → röle tıklar. Folyoya dokun → röle değişir, kumanda ekranı takip eder.

   Folyo tepki vermiyor ya da çok hassassa: `firmware/hub/src/main.cpp` içinde `TOUCH_DEBUG`'ı `1` yap, yeniden yükle, monitörde `deger=` / `baseline=` değerlerine dokunarak/bırakarak bak ve `TOUCH_DELTA_PCT`'yi ayarla. *ESP32-S3'te dokununca değer **yükselir**.* (Bu S3 hub sürümü yazar tarafından donanımda test edilmedi, README'deki durum tablosuna bak.)

## Aşama 5: Kumanda (Waveshare)

1. MX1.25 konnektöre **korumalı** bir LiPo pil bağla. **Önce kutbu kontrol et**: jenerik piller kartın konnektörüyle ters olabilir.
2. Yükle:
   ```bash
   cd firmware/controller
   pio run -t upload --upload-port <WAVESHARE_PORTU>
   ```
3. ✅ **Kontrol:** ekranda menü görünür. Seri log'da `[now] kumanda MAC = … sabit kanal = 1` (**`NOW_CHANNEL` ile aynı**). Hub çalışırken sol üstteki uyarı üçgeni birkaç saniye içinde kaybolur.
4. Sayfalar arasında kaydır, **MASA**'daki büyük düğmeye bas: hub'ın rölesi tıklar, düğme sarı/yeşile döner. **YATAK**'a bas: yatak düğümünün rölesi tıklar.

## Aşama 6: Uçtan uca test (hala 220V yok)

- [ ] Kumanda → **MASA** → hub rölesi tıklıyor, renkler doğru.
- [ ] Folyoya dokun → röle değişiyor, kumanda ekranı takip ediyor.
- [ ] Kumanda → **YATAK** → yatak rölesi tıklıyor.
- [ ] Kumanda → **HEPSİ** → iki röle de tepki veriyor.
- [ ] Kumandanın son sayfası (**menü sırası**) ile bir sayfayı oklarla taşı, yeniden başlatınca kalıyor.
- [ ] **Durum** sayfası: hub ve yatak için dBm görünüyor (yeşil, yaklaşık −65 dBm'den iyi).
- [ ] Hub'ın fişini çek: kumanda uyarı üçgenini gösteriyor ve basılan düğme ~2 sn sonra eski haline dönüyor.

Bir şey olmazsa [troubleshooting.md](troubleshooting.md).

## Aşama 7: Lambaları bağla (220V)

Ancak şimdi. **Önce [hardware.md → 220V tarafı](hardware.md#220v-tarafı-güvenlik) bölümünü tamamen oku.** Özet: sigorta kapalı, röleden **sadece faz** geçer (`COM` giriş, `NO` çıkış), nötr röleyi atlar, hepsi kapalı yalıtkan kutuda, ilk denemede lamba prizde değil.

Bağladıktan sonra Aşama 6'daki röle testlerini lambayla tekrarla: **kumanda "kapalı" derken lamba sönük olmalı** ve **ESP'nin fişini çekip taktığında (elektrik kesintisi gibi) lamba kapalı başlamalı.** Lamba ters çalışıyorsa `NC`'ye bağlıdır: `NO`'ya taşı.

🎉 Bitti.
