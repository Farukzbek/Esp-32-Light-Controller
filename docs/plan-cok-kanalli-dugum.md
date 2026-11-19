# Plan: çok kanallı düğüm (bir ESP32'de hem servo hem röle, PIR...)

Durum: **yalnızca plan** (kullanıcı fikri, 2026-11-19: "bir ESP32 hem servo hem röle sürebilmeli"). Donanım olarak mümkün: ESP32'nin birçok GPIO'su var, röle bir pine, servo başka bir pine bağlanır; servoyu ayrı 5V kaynaktan beslemek ve GND'leri birleştirmek yeter. Bugünkü yazılımda **bir düğüm = bir rol**.

## Neden bugün tek rol

- Eşleşmede düğüme tek bir `role` verilir (`PairSecret.reserved[0]`), düğüm tek pini kullanır.
- Köprüde bir **yuva = bir ESP-NOW bağlantısı = bir düğüm** (en çok 5–7 yuva, ESP-NOW'da en çok 6 şifreli eş).

## Çözüm: kanal = cihaz, bağlantı = düğüm

`NowMsg.dev` zaten "hangi cihaz" alanı. Düğüm başına **tek ESP-NOW bağlantısı** kalır; bir düğüm birden çok **kanal** (cihaz kimliği) sunar. Bu ESP-NOW'daki 6 eş sınırını da aşar: 6 düğüm × birkaç kanal.

| Parça | Değişiklik |
|---|---|
| Düğüm | Kanal listesi (NVS): her kanalın rolü, pini, ayarı. Aynı `dev` yuva numarası farklı kanallar için ayrı yuvalar (`d3`, `d4`...) olarak tanınır |
| Köprü | `linkOfDev(dev)`: yuva → bağlantı eşlemesi bir tabloya çıkar (şu an yuva = bağlantı sırası). Aynı düğümün kanalları aynı `SpanPoint`'i paylaşır |
| Eşleşme | İlk kanal eşleşmede seçilen roldür; "Kanal ekle" ile aynı düğüme ikinci/üçüncü kanal eklenir (rol + pin) |
| Uygulama | Cihazlarım'da düğüm altında kanallar listelenir; "Kanal ekle" düğmesi. Her kanal bugünkü cihaz gibi görünür (ışık karosu, servo karosu...) |
| Protokol | Yeni mesaj gerekmez: kanal ekleme/silme için küçük bir `NOW_CHANNEL_CFG` (yuva, rol, pin) yeterli; `PAIR_VERSION` aynı kalabilir |

## Dikkat

- Aynı düğümde iki kanal aynı pini kullanamaz (uygulama engellemeli, düğüm de reddetmeli).
- Servo kanalı varken PIR gibi gürültülü bir girişi aynı güç hattına bağlamak sorun çıkarabilir (servo akım çeker): donanım notu belgeye eklenir.
- Düğüm kablosuz güncellemesi (OTA) tek düğümü günceller, kanallar etkilenmez; **OTA planıyla birlikte** yapılırsa protokol bir kere kırılır (bkz. plan-dugum-ota.md, karar 1–2).

## Önerilen sıra

Düğüm OTA (protokol v3) ile birlikte tasarla; ardından kanal listesi + "Kanal ekle". Kod henüz yok.
