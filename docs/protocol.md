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

**Kendiliğinden bildirim:** masa düğümünde dokunma bandı ile durum değişince düğüm `STATE{seq=0}` gönderir (kumanda uykudaysa atlar).

## Hangi cihaz hangi `dev`'e sahip?

| `dev` | Sahibi | Kumanda nereye gönderir |
|---|---|---|
| `DEV_MASA` (0) | Hub | Hub |
| `DEV_YATAK` (1) | Yatak düğümü | Yatak düğümü |
| `DEV_LED` (2) | Hub (şimdilik sanal durum, gerçek LED düğümü yok) | Hub |

## Yeni bir düğüm eklemek (örn. LED şerit)

1. `now_config.h`'a yeni düğümün MAC'ini ekle (`NOW_LED_MAC`).
2. Düğüm, yatak düğümünün `main.cpp`'sini örnek alır: `SpanPoint`, radyo kilidi (`setChannelMask(1 << NOW_CHANNEL)`), `SET`'te çıkışı sür, `STATE` ile yanıtla, sahibi olmadığı cihazlar için `NOW_UNKNOWN` gönder.
3. `firmware/controller-cyd/src/now_link.cpp`: `LINKS`'i artır, yeni `SpanPoint`'i ekle, `linkOfDev()` içinde `DEV_LED`'i yeni linke yönlendir, hub tarafındaki sanal LED durumunu kaldır.
4. Renk/parlaklık gerekiyorsa `NowMsg`'a alan ekle (boyutu iki uçta birlikte değiştir) ya da yeni bir mesaj tipi tanımla.


## Roller: servo ve PIR (2026-11-17)

Eslesmede kopru, dugumun rolunu secer (`PairSecret.reserved[0]`: 0 isik, 1 servo, 2 PIR; eski dugum 0 gorur = isik). Hepsi mevcut 10 baytlik `NowMsg` icindedir, **protokol surumu (PAIR_VERSION 2) degismedi**.

| Mesaj | Yon | Alanlar |
|---|---|---|
| `NOW_SERVO_CFG` (6) | kopru -> dugum | `dev` yuva, `val` pin, `st[0]` bit0 ters yon, `st[1]` kapali acisi, `st[2]` acik acisi, `st[3]` hiz (10 derece/sn; 0 = aninda) |
| `NOW_SERVO_CFG2` (7) | kopru -> dugum | `val` birakma suresi (100 ms; 0 = asla birakma), `st[0]` en kucuk aci, `st[1]` en buyuk aci |
| `NOW_SERVO_GOTO` (8) | kopru -> dugum | `val` hedef aci (0..180), `st[0]` hiz (`NOW_UNKNOWN` = ayardaki) |
| `NOW_SERVO_FREE` (9) | kopru -> dugum | servoyu hemen birak (PWM kesilir, elle oynatilabilir) |
| `NOW_PIR_CFG` (10) | kopru -> dugum | `val` pin, `st[0]` tutma (sn), `st[1]` onay (10 ms), `st[2]` bekleme (sn), `st[3]` bit0 ters |
| `NOW_ANGLE` (12) | dugum -> kopru | `val` HEDEF aci (son komut), `st[0]` bit0 serbest, **bit1 hareket ediyor**, `st[1]` su anki aci, `seq` ilgili komutun numarasi. Dugum komutta ve hedefe varinca (ya da aninda harekette 800 ms sonra) bildirir; kopru `moving` bildirimi 2 dk gelmezse artik hareket saymaz |

- **Servo:** `NOW_SET` (ac/kapat) ayarli iki aciya gider; boylece otomasyon, Siri ve widget servoyu olduklari gibi kullanir. Hedefe varinca birakma suresi sonra PWM kesilir ve pin asagi cekilir: servo karsi koymaz. Konum geri bildirimi yoktur; bildirilen aci son komut verilen acidir.
- **PIR:** hareket `NOW_STATE` mesajinin `val` alaninda gelir (1 hareket, 0 yok). Filtre (onay, tutma, bekleme) dugumdedir (`firmware/shared/role_logic.h`). Algilama acisi/menzili cihazin kendisidir, yazilimdan degismez.
- **"Tum isiklar"** yalniz isik rolundeki cihazlara gider; servo ve PIR katilmaz.
- **Kopru API:** `POST /api/node/servo`, `POST /api/servo` (aci), `POST /api/servo/free`, `POST /api/node/pir`, `POST /api/pair/accept?...&role=servo|pir|light`; `GET /api/state` artik `roles` ve `servo` (`{"d3":{"angle":90,"free":true}}`) dondurur.
- **Masa ve yatak dugumleri** (eski hub/bed-node) artik `NOW_CFG` ile role pinini/mantigini uygulamadan alir (yalniz telefon koprusunden, NVS'te saklanir).

## Otomasyon kurali: gunler

Kapi dugumu kural metni: `en:eylem:isiklar:bas:bit:bekleme[:gunler]`. `gunler`: bit0 Pazartesi ... bit6 Pazar (1..127); yoksa her gun. Gece yarisini asan aralikta gun, aralik **basladigi** gundur (`firmware/shared/rule_days.h`). Eski kapi yazilimi 7. alani reddeder; uygulama bu alani yalnizca her gun disindaysa gonderir.


## Zamanlı programlar (köprü API'si)

`GET /api/schedules` → `{"rules":"...","tz":180,"synced":true,"time":"09:41","count":1}`; `POST /api/schedules?rules=en:eylem:cihazlar:dakika:gunler:aci;...&tz=180` (UTC'den dakika farkı). Eylem: 0 çevir, 1 aç, 2 kapat, 3 servo açısı. Cihazlar: bit = 1 << yuva. Günler: bit0 Pzt ... bit6 Paz. En çok 16 kural, NVS'te saklanır, köprü NTP ile saati alır ve her dakika kurallara bakar; elektrik kesintisinde kaçırılanlar atlanır (`firmware/shared/schedule.h`).
