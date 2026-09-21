# Mimari ve tasarım kararları

## Roller

| Cihaz | Görev | WiFi'ye bağlı mı? |
|---|---|---|
| **Kumanda** (Waveshare AMOLED) | Kullanıcı arayüzü, komut gönderir, durumu gösterir | Hayır (sadece ESP-NOW, WiFi radyosu açık ama bir ağa bağlı değil) |
| **Hub** (ESP32-S3 Super Mini) | Apple Home lambası + dokunma bandı + röle; kumandanın ESP-NOW karşılığı | **Evet** (HomeKit için) |
| **Yatak düğümü** (ESP32-S3 Super Mini) | Röleyi sürer | **Hayır** |

Neden hub? HomeKit sürekli açık bir WiFi bağlantısı ister, pilli bir kumanda için uygun değildir. Bu yüzden Apple Home'a bağlı tek cihaz prizde sürekli çalışan hub'dır. Diğerleri modemle hiç konuşmaz, bu da modeme yük bindirmez ve internet gitse bile lambaların çalışmasını sağlar.

## Veri akışı

```
Kumanda ekranında düğmeye basılır
   -> UI durumu hemen gösterir (iyimser)
   -> now_send_set(dev, 0/1) kuyruğa girer (ayrı FreeRTOS görevi, arayüz hiç beklemez)
   -> SpanPoint::send() ile hedef cihaza gider (masa/LED -> hub, yatak -> yatak düğümü)
   -> hedef röleyi sürer, aynı seq numarasıyla STATE yanıtı döner
   -> kumanda yanıtı görünce onaylı durumu günceller
   -> 350 ms'de yanıt yoksa yeniden dener (3 kez); hala yoksa ekran onaylı duruma döner
      ve sol üstte uyarı çıkar (hangi cihaz sustu yazar)
```

Hub'da bir şey değişirse (dokunma bandı ya da Apple Home) hub `STATE` mesajını kendiliğinden kumandaya iter (`seq = 0`). Kumanda uykudaysa hub bunu atlar (son 25 sn'de kumandadan ses gelmediyse).

## Güvenilirlik kararları

- **`SET` mesajları mutlak değer taşır** ("aç" değil "durum = 1"). Aynı mesaj birkaç kez ulaşsa da (ESP-NOW yeniden denemeleri) sonuç değişmez.
- **Gerçek onay uygulama seviyesindedir.** `SpanPoint::send()` fonksiyonunun `true` dönmesi, karşı tarafın *uygulamasının* mesajı aldığı anlamına gelmez, sadece karşı radyonun 802.11 donanım onayı gelmiştir. Bu yüzden komut, karşı tarafın aynı `seq` ile döndürdüğü `STATE` mesajı ile onaylanır.
- **`NOW_UNKNOWN (0xFF)`**: bir düğüm `STATE` içinde sahibi olmadığı cihazlar için 0xFF gönderir. Kumanda bunları atlar. Böylece yatak düğümü kapalıyken yatak lambasının "kapalı" görünmesi engellenir, son bilinen durum korunur.

## Kanal yönetimi (en kritik kısım)

ESP-NOW cihazları aynı WiFi kanalında olmalıdır. Hub bir router'a bağlı olduğu için router'ın kanalında çalışır. Diğer iki cihaz bu kanalı bulmalıdır.

`SpanPoint` kütüphanesi, karşı taraftan yanıt alamayan uzak cihazda **tüm kanalları sırayla tarar** (her kanalda 3 deneme) ve bunu kalıcı depoya (NVS) yazar. Denediğimizde bunun üç sorunu çıktı:
1. Uykudaki bir cihaza gönderirken radyo, hub'ın kanalından kopuyordu.
2. Her taramada flash'a yazıldığı için NVS yıpranıyordu.
3. Yatak düğümü tarama sırasında ~1 sn "sağır" kalıyordu.

Çözüm (bkz. `firmware/controller/src/now_link.cpp`, `firmware/bed-node/src/main.cpp`):
- Radyo, `SpanPoint::setChannelMask(1 << kanal)` ile **tek kanala kilitlenir**. Yanıt vermeyen cihaz sadece hızlıca "başarısız" olur, kanal taramaz.
- Doğru kanal, mesaj onaylarına güvenmeden, **router'ın SSID'sinin WiFi taramasıyla** okunur (`esp_wifi_scan_start` ile o SSID'yi filtreleyip kanalına bakılır, NVS'ye yazmaz). Açılışta ve hub uzun süre sessiz kalırsa seyrek yapılır.
- Yatak düğümü kontrolcüyle konuşurken (son 8 sn'de mesaj varsa) tarama yapmaz.

Bu yüzden `now_config.h` içindeki `NOW_ROUTER_SSID`, **hub'ın gerçekte bağlandığı** ağ adı olmalıdır (powerline/repeater varsa onun yayın adı) ve router'ın kanalını sabitlemeni öneririz (1, 6 ya da 11).

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
