# ESP-NOW protokolü

Tanım: [`firmware/shared/now_proto.h`](../firmware/shared/now_proto.h). Kumanda, hub ve yatak düğümü **aynı dosyayı** kullanır, mesaj boyutu iki uçta aynı olmak zorundadır (`SpanPoint` bunu doğrular).

Taşıma katmanı: HomeSpan'ın [`SpanPoint`](https://github.com/HomeSpan/HomeSpan/blob/master/docs/NOW.md) sınıfı (ESP-NOW üzerinde, MAC ile eşleşen, şifreli noktadan noktaya bağlantı; en fazla 7 şifreli bağlantı).

## Mesaj: `NowMsg` (9 bayt, packed)

| Alan | Boyut | Açıklama |
|---|---|---|
| `type` | 1 | `NOW_SET = 1`, `NOW_QUERY = 2`, `NOW_STATE = 3` |
| `dev` | 1 | `SET` için hedef cihaz: `DEV_MASA = 0`, `DEV_YATAK = 1`, `DEV_LED = 2` |
| `val` | 1 | `SET` için değer: 0 kapalı, 1 açık |
| `st[4]` | 4 | `STATE` için cihaz durumları: `[masa, yatak, led, rezerve]`. **`0xFF` (`NOW_UNKNOWN`) = gönderen bu cihazın sahibi değil, atla** |
| `seq` | 2 | İstek numarası. `STATE` yanıtı, yanıtladığı isteğin `seq`'ini taşır. Cihazın kendiliğinden gönderdiği bildirimlerde 0 |

## Akışlar

**Komut:** kumanda → `SET{dev, val, seq=N}` → cihaz röleyi sürer → cihaz → `STATE{st, seq=N}`. Kumanda `seq=N` yanıtını görünce komutu onaylanmış sayar. 350 ms'de gelmezse aynı `SET` yeniden gönderilir (en fazla 3 deneme), sonra hata sayılır.

**Sorgu:** kumanda → `QUERY{seq=N}` → cihaz → `STATE{seq=N}`. Kumanda her 10 sn'de ve ekran uyanınca sorar. Bu aynı zamanda bağlantı denetimidir (30 sn yanıt yoksa uyarı çıkar).

**Kendiliğinden bildirim:** hub'da dokunma bandı ya da Apple Home ile durum değişince hub `STATE{seq=0}` gönderir (kumanda uykudaysa atlar).

## Hangi cihaz hangi `dev`'e sahip?

| `dev` | Sahibi | Kumanda nereye gönderir |
|---|---|---|
| `DEV_MASA` (0) | Hub | Hub |
| `DEV_YATAK` (1) | Yatak düğümü | Yatak düğümü |
| `DEV_LED` (2) | Hub (şimdilik sanal durum, gerçek LED düğümü yok) | Hub |

## Yeni bir düğüm eklemek (örn. LED şerit)

1. `now_config.h`'a yeni düğümün MAC'ini ekle (`NOW_LED_MAC`).
2. Düğüm, yatak düğümünün `main.cpp`'sini örnek alır: `SpanPoint`, radyo kilidi + SSID taraması, `SET`'te çıkışı sür, `STATE` ile yanıtla, sahibi olmadığı cihazlar için `NOW_UNKNOWN` gönder.
3. `firmware/controller/src/now_link.cpp`: `LINKS`'i artır, yeni `SpanPoint`'i ekle, `linkOfDev()` içinde `DEV_LED`'i yeni linke yönlendir, hub tarafındaki sanal LED durumunu kaldır.
4. Renk/parlaklık gerekiyorsa `NowMsg`'a alan ekle (boyutu iki uçta birlikte değiştir) ya da yeni bir mesaj tipi tanımla.
