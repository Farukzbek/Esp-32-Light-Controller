# Waveshare ESP32-S3-Touch-AMOLED-1.64 notları (V1)

Bu kartla çalışırken öğrendiklerimiz. Waveshare'in wiki'si pinleri ve bazı ayrıntıları vermiyor, çoğu şemadan ve deneyerek bulundu.

## Ekran

- **Sürücü CO5300**, 280×456, QSPI. Espressif'in `esp_lcd_sh8601` sürücüsüyle çalışır (`firmware/controller/src/esp_lcd_sh8601.c`). Çizimde **x ekseninde 0x14 (20 piksel) kaydırma** gerekir (`lcd_bsp.c` içinde `+ 0x14`).
- Panel her güncellemenin **çift satırdan/sütundan başlayıp tek satırda bitmesini** ister. Bunun için `rounder_cb` alanı yuvarlar.
- **Çizim tamponunun satır sayısı ÇİFT olmalı.** LVGL'in kısmi çizimi tampon yüksekliği kadar dilimler, dilim tek satırlıysa panel güncellemesi bozulur (görüntü yenilenmiyor gibi görünür). Bu projede tampon 280×76 piksel: dikeyde 76, yatayda (456 genişlik) 46 satır, ikisi de çift. Tamponu değiştirirsen hem dikey hem yatay çözünürlükte çift satır çıkmasına dikkat et (`lcd_config.h`).
- **LVGL'in `sw_rotate`/`transform_angle` özelliği bu panel sürücüsüyle bozuk görüntü üretti.** Yatay mod, `lcd_bsp.c` içindeki flush fonksiyonunda alanı 90° döndürerek ve dokunma koordinatlarını eşleyerek yapılır. Çalışma anında `lv_disp_drv_update` ile çözünürlük 280×456 ↔ 456×280 değiştirilir.
- Parlaklık `0x51` komutuyla ayarlanır, `0` yapınca AMOLED siyah kalır (yanma olmaz, tüketim çok düşük).

## Dokunmatik

- **FT3168**, I²C 0x38, SDA 47 / SCL 48. **V1'de INT pini yok**: dokunma sürekli sorgulanır (LVGL 30 ms) ve derin uykuda 300 ms'de bir uyanılır.
- Çip arada tek tük boş/hatalı okuma döndürebilir. Her boş okumayı "parmak kalktı" saymak kaydırma ve tıklamayı iptal eder. Bu yüzden en fazla 2 ardışık boş okuma yok sayılıp son noktada basılı tutulur (`lcd_bsp.c`, `example_lvgl_touch_cb`).

## IMU (QMI8658)

- I²C 0x6B. Bu projede **sadece jiroskop (gz) ve ivmeölçerin Z ekseni** kullanılır. Yazarın kartında ivmeölçerin **Y ekseni ham `-32768`'de takılı kaldı** (her ölçüm aralığında, güç kesince de düzelmedi) ve X ekseni ~+1 g ofsetli okudu, bu yüzden yerçekimi vektörü güvenilir değil. Jiroskop ve Z ekseni sağlamdı. Bunun sebebini bilmiyoruz (bir donanım arızası olabilir). Kendi kartında ivmeölçer sağlamsa daha basit bir yön algılama yazabilirsin.
- Jiroskop bias'ı (yaklaşık −0,6 dps) açılışta ~0,7 sn'de ölçülür, cihaz sabit varsayılır. Uykudan uyanınca (elde olabileceği için) RTC belleğindeki son bias kullanılır.
- Yön algılaması hareket bazlıdır: ekran normali (gz) etrafındaki dönüş entegre edilir, ≥45°'lik bir hareket bir adım sayılır, mutlak açı tutulmaz.

## Batarya ve şarj (V1 şemasından)

- Şarj IC'si **ETA6098**, giriş `USB_5V` (VBUS, Schottky diyottan ÖNCE). Güç yolu **Q1 (AO3401 P-MOSFET)**: USB varken pil sistemden ayrılır, sistem VBUS'tan diyotla beslenir. USB yokken sistem pilden beslenir.
- **Şarj durum çıkışı (STAT) sadece kart üstündeki LED'e gider, MCU'ya bağlı değildir.** Pil takılıyken bile firmware'den "şarj oluyor" bilgisi doğrudan alınamaz.
- **GPIO4 (BAT_ADC), pilin değil sistem hattı `VCC`'nin 200K/100K ile bölünmüş halidir** (÷3). Bu, harici güç tespiti için kullanılabilir: pilde `VCC` ≤ ~4,2 V, harici 5V varken ~4,5–4,9 V. Kodda eşik `PWR_EXT_ON_V = 4.35`, `PWR_EXT_OFF_V = 4.30` (histerezis). Pil takılı değilken USB'de 4,88 V, pil takılı şarjda 4,5 V okundu.
- Bu yöntemle "pil yüzdesi" **tahmindir** (3,30 V→0, 4,15 V→100 doğrusal), gerçek bir pil ölçer çipi yoktur.
- **Kartta pil koruma devresi (aşırı deşarj, kısa devre) yoktur.** Korumalı (PCM'li) pil kullan. MX1.25 konnektör polaritesi jenerik pillerle ters olabilir, takmadan önce kontrol et.

## Şarj simgesi

Sağ üstte beyaz şimşek simgesi, yukarıdaki `VCC` ölçümü harici güç gösterdiğinde çıkar. "Şarj oluyor" ile "pil dolu" arasını ayırmaz.
