# Donanım, pinler ve kablolama

> 🇬🇧 English version: [en/hardware.md](en/hardware.md)

## Malzeme listesi

| Adet | Parça | Not |
|---|---|---|
| 1 | Waveshare **ESP32-S3-Touch-AMOLED-1.64** | Kumanda. Bu depo **V1 pinout**'u için yazıldı (aşağıya bak) |
| 2 | ESP32-S3 Super Mini | Hub ve yatak düğümü |
| 2 | 1 kanallı 5V röle modülü | **Ters mantıklı (active-low)** tip: IN=LOW iken çalışır. Optokuplörlü |
| 1 | Korumalı LiPo pil (PCM'li), MX1.25 konnektörlü | Kumanda için. Kartta koruma devresi **yok**. Konnektör polaritesini kontrol et |
| 1 | Alüminyum folyo bant + iki tel | Hub'ın dokunma bandı |
| 2 | 5V USB adaptör (≥1 A) + USB-C kablo | Hub ve yatak düğümü için |
| — | Kapalı, yalıtkan kutu, klemens, kablo (≥0,75 mm²) | 220V bağlantıları için |

## Kumanda: Waveshare ESP32-S3-Touch-AMOLED-1.64 (V1)

Kart arkasında V1/V2 yazmayabilir. Waveshare iki revizyon çıkardı, pinleri farklıdır. V1 için pinler `firmware/controller/include/lcd_config.h` içindedir:

| Sinyal | V1 GPIO | V2'de |
|---|---|---|
| LCD CS | **9** | 46 |
| LCD CLK / D0 / D1 / D2 / D3 | 10 / 11 / 12 / 13 / 14 | aynı (kontrol et) |
| LCD RST | 21 | aynı (kontrol et) |
| Dokunmatik SDA / SCL | **47 / 48** (I²C 0x38, FT3168) | farklı |
| Dokunmatik INT | **yok** | GPIO 18 |
| IMU (QMI8658) | I²C 0x6B, INT1 = GPIO 46 | INT1 = GPIO 9 |
| BAT_ADC | GPIO 4 | — |

V2 kartın varsa `lcd_config.h` içindeki pinleri Waveshare'in V2 örneğine göre değiştirmen gerekir. Ayrıntılar: [waveshare-amoled-notes.md](waveshare-amoled-notes.md).

### Kartın harici konnektörü (P2, V1 şemasından)

| P2 pini | Sinyal |
|---|---|
| 8 | 3V3 |
| 9 | VBAT |
| 10 | GND |
| 11 | **USB_5V** (VBUS, şarj IC'sinin girişi ve sistem besleme yolu) |

Buraya 5V verirsen kart tıpkı USB takılmış gibi beslenir ve pil şarj olur. Kaynak **≤5,5 V** olmalı ve hat **korumasızdır**: mıknatıslı dock gibi dışarıdan erişilebilir kontaklar için sigorta (polyfuse), TVS ve ters bağlanma koruması ekle.

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
- Kumanda: pil ve/veya USB-C ya da dock. Pilde çalışırken sistem hattı ≤4,2 V, harici 5V varken ≈4,5–4,9 V olur. Şarj simgesi ve durum sayfası bunu kullanır (bkz. [waveshare-amoled-notes.md](waveshare-amoled-notes.md)).
- **Mıknatıslı dock fikri:** iki neodyum mıknatıs hem tutucu hem kontak olabilir. Zıt kutuplarla yerleştirirsen ters takılamaz (mıknatıslar iter). Kontakları P2'nin `USB_5V` (11) ve `GND` (10) pinlerine bağla, arada sigorta + TVS kullan. Mıknatıs ve metal kontakları ESP32 modülünün **anteninden uzak** tut. IMU'nun manyetometresi olmadığı için mıknatıs yön algılamayı bozmaz.
