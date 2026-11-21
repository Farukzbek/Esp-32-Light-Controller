# Donanım, pinler ve kablolama

> 🇬🇧 English version: [en/hardware.md](en/hardware.md)

## Malzeme listesi

| Adet | Parça | Not |
|---|---|---|
| 1 | **ESP32-2432S028R ("CYD")**, 2,8" 320x240 dokunmatik | Kumanda paneli (USB ile beslenir). `firmware/controller-cyd` bu kart için yazıldı |
| 2 | ESP32-S3 Super Mini | Hub ve yatak düğümü |
| 2 | 1 kanallı 5V röle modülü | **Ters mantıklı (active-low)** tip: IN=LOW iken çalışır. Optokuplörlü |
| 1 | Alüminyum folyo bant + iki tel | Hub'ın dokunma bandı |
| 3 | 5V USB adaptör (≥1 A) + kablo | Hub, yatak düğümü ve CYD paneli için |
| — | Kapalı, yalıtkan kutu, klemens, kablo (≥0,75 mm²) | 220V bağlantıları için |

## Kumanda paneli: CYD (ESP32-2432S028R)

2,8" 320x240 dokunmatik ekranlı hazır kart. Pinleri kartın üzerinde sabittir, kablo çekmen gerekmez. `firmware/controller-cyd` bu pinlere göre yazıldı:

| Parça | Pin |
|---|---|
| Ekran (ILI9341, HSPI) | MOSI 13, MISO 12, SCLK 14, CS 15, DC 2 |
| Arka ışık (PWM) | GPIO 21 |
| Dokunmatik (XPT2046, ayrı SPI hattı) | CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36 |
| Işık sensörü (LDR) | GPIO 34 (ters çalışır: ışık artınca voltaj düşer, yazılım telafi eder) |

Notlar:
- Ekran paneli TFT_eSPI'nin `ILI9341_2_DRIVER` varyantı + ters renkle çalışır (ayarlar `platformio.ini` içinde). Renkler ters ya da görüntü bozuksa önce bunu kontrol et.
- Kartta pil/şarj devresi yoktur: USB ile beslenir (≥1 A adaptör yeterli). Bu yüzden derin uyku yoktur; ekran yalnızca arka ışığı kapatır.
- İlk açılışta dört köşedeki hedeflere dokunarak kalibrasyon yapılır (Ayarlar → DURUM'dan tekrarlanır).
- Kartın MAC adresi `NOW_CONTROLLER_MAC` olarak hub ve yatak düğümünün `now_config.h` dosyasında da yazılı olmalıdır.

## Hub ve yatak düğümü (ESP32-S3 Super Mini)

| Sinyal | Hub | Yatak düğümü |
|---|---|---|
| Röle `IN` | **GPIO 5** | **GPIO 5** |
| Röle `VCC` | 5V | 5V |
| Röle `GND` | GND | GND |
| Dokunma bandı | **GPIO 4** (T4) | — |

Pinler `firmware/hub/src/main.cpp` ve `firmware/bed-node/src/main.cpp` başındaki `#define`'larla değişir. Açılışta özel görevi olan pinleri (GPIO 0, 3, 45, 46) ve USB pinlerini (19, 20) kullanma. Röle `VCC`'yi **3V3'ten değil 5V pininden** ver (bobin 70–90 mA çeker).

Röle çıkışı, açılışta pin `OUTPUT` yapılmadan önce "kapalı" seviyesine getirilir, böylece elektrik gelince röle bir an çalışmaz.

### Hub dokunma bandı

Kapasitif dokunma, açılıştaki 3 saniyelik kalibrasyonda (**bu sırada bantına dokunma**) baseline ölçer, sonra baseline'dan %`TOUCH_DELTA_PCT` sapmayı dokunma sayar (3 ardışık okuma + 400 ms bekleme). Yavaş bir kayma takibi vardır.

**ESP32-S3 uyarısı:** klasik ESP32'de dokununca okuma değeri **düşer**, ESP32-S3'te **yükselir**. Kod bunu `TOUCH_RISES_ON_TOUCH` ile ayırır, ancak S3 hub'ı **donanımda test edilmemiştir**. Eşik, bant boyutuna ve kablo uzunluğuna göre değişir. Ayarlamak için:
1. `firmware/hub/src/main.cpp` içinde `TOUCH_DEBUG`'ı `1` yap, yükle.
2. Seri monitörde dokunmadan ve dokunarak `deger=` / `baseline=` değerlerine bak.
3. `TOUCH_DELTA_PCT`'yi buna göre ayarla (küçük değer daha hassas, ama parazite açık).

## Röle klemensi: hangi uç ne?

Çoğu modülde 3 vidalı uç vardır: **NC – COM – NO** (sıra modüle göre değişir, etiketi olmayabilir). Multimetreyle **bulmak** gerekir, tahmin etme. **220V'a hiçbir şey bağlı değilken:**

1. Multimetreyi süreklilik ("bip") konumuna al. Röle enerjisizken uçları ikişerli dene: **bip yapan çift COM + NC**, bip yapmayan **NO**.
2. Rölenin `VCC`'sine 5V, `GND`'sine GND ver ve `IN`'i GND'ye değdirerek çalıştır (tık sesi, modül LED'i). Bu sırada bip yapan çift **COM + NO**.
3. **İki durumda da bip yapan uç COM'dur**, sadece çalışırken bip yapan NO, sadece enerjisizken bip yapan NC.

## 220V tarafı (güvenlik)

> **Uyarı:** Şebeke gerilimi öldürebilir. Emin değilsen bir elektrikçiye yaptır. Bu bölüm bir garanti ya da elektrik tesisatı talimatı değildir.

- **Sigortayı kapat**, lambayı prizden çek, bağlantıyı bitirmeden hiçbir şeyi elektriğe verme.
- Lambanın kablosunda **sadece bir damarı (faz)** ortadan kes. Fiş tarafındaki uç **COM**'a, lamba tarafındaki uç **NO**'ya gider. **NC boş kalır.** Diğer damar (nötr) ve varsa toprak **röleden geçmez**, doğrudan lambaya gider.
  ```
  Fiş faz ----------> COM   (röle)   NO ----------> lamba fazı
  Fiş nötr ---------------------------------------> lamba nötrü
  ```
- **NO** kullan: röle enerjisizken (ESP kapalıyken, elektrik gidip gelince) lamba **kapalı** kalır. NC'ye bağlarsan lamba açık başlar ve yazılımdaki açık/kapalı ters görünür.
- Faz hangi damar? Gözle anlaşılmaz: kalem tipi voltaj test cihazı kullan. Emin değilsen ampul değiştirirken **fişi çek** (nötrü kesmiş olursan lamba kapalıyken bile soket içinde 220V kalır).
- Tüm 220V bağlantılarını **kapalı, yalıtkan bir kutuda** yap, vidaları sıkıp kabloları hafifçe çekerek kontrol et, kabloya gergi payı bırak. Düşük gerilim (ESP) kablolarını 220V'tan ayır.
- Rölenin etiketindeki akım/gerilim değerini kontrol et (LED ampul için 10 A / 250 VAC fazlasıyla yeter).
- İlk denemeyi 220V bağlıyken değil, önce **sadece 5V tarafıyla** yap (rölenin tık sesini dinle).

## Güç

- Hub ve yatak düğümü: prize takılı **tek bir 5V/≥1 A adaptör + USB-C kablo**. ESP, röle `VCC`'yi de kendi `5V` pininden besler, ayrı kablo gerekmez. ESP'yi hem USB'den hem de başka bir yerden 5V pinine aynı anda besleme.
- Kumanda paneli (CYD): USB'den 5V (≥1 A adaptör). Kartta pil yok.
