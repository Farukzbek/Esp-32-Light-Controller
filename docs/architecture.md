# Mimari ve tasarım kararları

## Roller

| Cihaz | Görev | WiFi'ye bağlı mı? |
|---|---|---|
| **Kumanda** (Waveshare AMOLED) | Kullanıcı arayüzü, komut gönderir, durumu gösterir | Hayır |
| **Masa düğümü / hub** (ESP32-S3 Super Mini) | Dokunma bandı + röle; kumandanın ESP-NOW karşılığı | Hayır |
| **Yatak düğümü** (ESP32-S3 Super Mini) | Röleyi sürer | Hayır |

Üç cihaz da yalnızca ESP-NOW konuşur: router, modem, internet ya da bulut gerekmez, bir şey kesilse lambalar çalışmaya devam eder. Cihazlar bir ağa bağlanmadığı için WiFi ağ listesinde de görünmez (yalnızca istasyon modu). Apple Home/HomeKit desteği bilinçli olarak **yoktur**: HomeKit sürekli açık bir WiFi bağlantısı ister ve kanalın router'a bağlanmasına yol açar (bu projenin ilk sürümü böyleydi, aşağıya bak).

## Veri akışı

```
Kumanda ekranında düğmeye basılır
   -> UI durumu hemen gösterir (iyimser)
   -> now_send_set(dev, 0/1) kuyruğa girer (ayrı FreeRTOS görevi, arayüz hiç beklemez)
   -> SpanPoint::send() ile hedef cihaza gider (masa/LED -> masa düğümü, yatak -> yatak düğümü)
   -> hedef röleyi sürer, aynı seq numarasıyla STATE yanıtı döner
   -> kumanda yanıtı görünce onaylı durumu günceller
   -> 350 ms'de yanıt yoksa yeniden dener (3 kez); hala yoksa ekran onaylı duruma döner
      ve sol üstte uyarı çıkar (hangi cihaz sustu yazar)
```

Masa düğümünde bir şey değişirse (dokunma bandı) düğüm `STATE` mesajını kendiliğinden kumandaya iter (`seq = 0`). Kumanda uykudaysa düğüm bunu atlar (son 25 sn'de kumandadan ses gelmediyse).

## Güvenilirlik kararları

- **`SET` mesajları mutlak değer taşır** ("aç" değil "durum = 1"). Aynı mesaj birkaç kez ulaşsa da (ESP-NOW yeniden denemeleri) sonuç değişmez.
- **Gerçek onay uygulama seviyesindedir.** `SpanPoint::send()` fonksiyonunun `true` dönmesi, karşı tarafın *uygulamasının* mesajı aldığı anlamına gelmez, sadece karşı radyonun 802.11 donanım onayı gelmiştir. Bu yüzden komut, karşı tarafın aynı `seq` ile döndürdüğü `STATE` mesajı ile onaylanır.
- **`NOW_UNKNOWN (0xFF)`**: bir düğüm `STATE` içinde sahibi olmadığı cihazlar için 0xFF gönderir. Kumanda bunları atlar. Böylece yatak düğümü kapalıyken yatak lambasının "kapalı" görünmesi engellenir, son bilinen durum korunur.

## Kanal yönetimi (en kritik kısım)

ESP-NOW cihazları aynı WiFi kanalında olmalıdır. Üç cihaz `now_config.h` içindeki **`NOW_CHANNEL`** kanalında (varsayılan 1) **sabit** buluşur. Router'a ya da başka bir ağa bağımlılık yoktur.

`SpanPoint` kütüphanesi, karşı taraftan yanıt alamayan uzak cihazda **tüm kanalları sırayla tarar** (her kanalda 3 deneme) ve bunu kalıcı depoya (NVS) yazar. Kilitlemeden önce bunun üç sorunu çıktı:
1. Uykudaki bir cihaza gönderirken radyo, diğer cihazların kanalından kopuyordu.
2. Her taramada flash'a yazıldığı için NVS yıpranıyordu.
3. Düğüm tarama sırasında ~1 sn "sağır" kalıyordu.

Çözüm (bkz. `firmware/controller/src/now_link.cpp`, `firmware/hub/src/main.cpp`, `firmware/bed-node/src/main.cpp`): radyo `SpanPoint::setChannelMask(1 << NOW_CHANNEL)` ile **tek kanala kilitlenir**. Yanıt vermeyen cihaz sadece hızlıca "başarısız" olur, kanal taramaz.

**Neden router'a bağlı kanal değil?** İlk sürümde hub WiFi'ye bağlıydı (HomeKit için) ve diğer cihazlar hub'ın kanalını router'ın SSID'sini tarayarak buluyordu. Router/powerline'ın WiFi'si kapatılınca ya da kanalı değişince cihazlar birbirini kaybetti. Sabit kanal bu sorun sınıfını ortadan kaldırır. Evinde aynı kanalı yoğun kullanan bir 2,4 GHz ağ varsa `NOW_CHANNEL`'ı değiştir (üç cihazı da yeniden yükle).

## Uyku stratejisi (kumanda)

| Süre | Durum |
|---|---|
| 0–15 sn | Tam parlaklık |
| 15 sn | Ekran kısılır |
| 60 sn | Ekran kapanır (dokunuş uyandırır, ilk dokunuş bir düğmeye basmaz) |
| 90 sn | **Derin uyku**, sadece pilde (USB/dock varken uyumaz) |

Waveshare V1 kartında dokunmatiğin kesme (INT) pini yoktur. Bu yüzden derin uykuda **300 ms'de bir** uyanıp dokunmaya bakılır (`main.cpp` içinde `setup()`'ın başında, ekran/radyo açılmadan). Dokunma yoksa hemen tekrar uyunur. Uykuda LCD'nin CS ve RST pinleri yüksek tutulur (yüzen CS paneli bozabilir). Uyandığında son yön (NVS), son lamba durumları ve jiroskop bias'ı (RTC belleği) geri gelir, hub'dan taze durum ayrıca sorulur.

Tahmini tüketim uykuda ortalama 7–10 mA (ölçülmedi), yani ~600 mAh pille birkaç gün.

## Arayüz notları

- **Sayfa sırası** kalıcıdır (NVS). Ayar sayfası ("Menü sırası") her zaman en sonda kalır.
- **Yön**: jiroskopla ekran normali etrafındaki dönüş entegre edilir, ≥45° bir hareket bir adım sayılır, mutlak açı tutulmaz (kayma birikmesin). Sırt üstü düz yatarken (Z ivmesi ≈1 g ve 1 sn boyunca) ya da kullanıcı kilitlediğinde dönme durur.
- LVGL'in kendi ekran döndürmesi bu panel sürücüsüyle bozuk görüntü verdi, bu yüzden döndürme `lcd_bsp.c` içindeki flush fonksiyonunda **pikseller elle döndürülerek** yapılır.
- Ekran fontu ASCII'dir (Ş, ı, Ğ yok).
