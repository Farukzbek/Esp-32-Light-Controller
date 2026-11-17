# Plan: ışık düğümlerini uygulamadan kablosuz güncelleme

Durum: **0. aşama hazır** (aşağıda), 1–5. aşamalar akşam evde, aşama aşama kodlanıp donanımda denenecek.

## 1. Hedef

Kullanıcı uygulamada bir ışığa dokunup **Yazılım → Güncelle**'ye basar. Düğüm yeni yazılımı alır, kendini yeniden başlatır, uygulama yeni sürümü gösterir. Düğümde WiFi şifresi olmaz, güncelleme kendiliğinden gelmez (sadece kullanıcı başlatınca), her düğüm ayrı güncellenir.

## 2. Mevcut durum (koddan okunanlar)

| Konu | Bulgu | Sonuç |
|---|---|---|
| Düğüm haberleşmesi | HomeSpan `SpanPoint`, mesaj **10 bayt** (`NowMsg`) | Yazılım aktarmaya yetmez (parça başına ~6 bayt) |
| SpanPoint sınırı | Bağlantı başına **sabit** mesaj boyutu, en fazla **200 bayt**; gelen mesaj boyutu tam eşleşmezse atılır; aynı MAC için ikinci bağlantı açılamaz (ilk eşleşen kazanır) | Büyük veri için bağlantının mesaj boyutunu büyütmek gerekir |
| Şifreli eş sınırı | ESP-NOW'da en fazla 6 şifreli eş. Genel köprüde 5 düğüm + 1 boş | Güncelleme için **ayrı ikinci bağlantı açılamaz**, mevcut bağlantı kullanılmalı |
| Düğüm bölümleri | `default.csv`: iki uygulama yuvası var ama **1280 KB** | OTA için yuvalar var |
| Düğüm yazılım boyutu | S3 1,21 MB, S2 1,22, klasik 1,23, **C3 1,33 MB, C6 1,34 MB** | **C3 ve C6 yuvaya (1,25 MB) sığmıyor**; şu an `app1`'in üstüne taşıyor. Bölüm tablosu büyütülmeli (`min_spiffs.csv`: 2 × 1,875 MB) |
| Köprü | Genel köprü `min_spiffs.csv` ile OTA yapabiliyor (0.5.0), kendi güncellemesini siteden HTTPS ile indiriyor | Aynı indirme kodu düğüm yazılımı için kullanılabilir |
| Kişisel köprü | `huge_app.csv`, **OTA yuvası yok** | Düğüm OTA testi için genel köprü gerekir (dev kit) |
| Rollback | Arduino'nun hazır önyükleyicisinde otomatik geri alma kapalı | Bozuk yazılım yüklenirse düğüm USB ile kurtarılır. Önlem: yazılımı değiştirmeden önce bütünlük doğrulaması |

## 3. Mimari

```
Uygulama --(HTTP, X-Token)--> Köprü --(ESP-NOW, şifreli, 200 B çerçeve)--> Düğüm
                                │
                                └-- siteden HTTPS ile yazılımı parça parça çeker (RAM'de tutmaz)
```

1. Uygulama köprüye "slot N'i güncelle" der.
2. Köprü, düğümün çip türüne göre siteden `esp32xx.ota.json` (sürüm, boyut, SHA-256) ve `esp32xx.ota.bin` (uygulama imajı) bilgisini alır.
3. Köprü imajı HTTP Range ile 4 KB'lık bloklar halinde indirir, 180 baytlık parçalara bölüp düğüme yollar.
4. Düğüm parçaları `esp_ota_write` ile boş yuvaya yazar, çalışan SHA-256'yı tutar.
5. Bitişte düğüm SHA-256'yı karşılaştırır, doğruysa `esp_ota_set_boot_partition` ile yuvayı değiştirir ve yeniden başlar.
6. Düğüm yeniden bağlanınca yeni sürümünü bildirir, uygulama "Güncel" gösterir.

Güvenlik: bütün trafik düğüm başına türetilmiş şifreyle (ESP-NOW LMK) gider, yani yalnızca eşleştiği köprü yazılım gönderebilir. Uygulama→köprü yetkisi mevcut token. İmaj bütünlüğü: manifestteki SHA-256 (HTTPS ile). **Bilinen açık:** imza yok; Cloudflare hesabı ele geçirilirse sahte yazılım dağıtılabilir. İleride imaj imzalama (ed25519) eklenebilir.

## 4. Protokol değişikliği (PAIR_VERSION 3)

**Kararım: yeni sürüm tek seferde kırıcı olsun.** Şu an yalnızca birkaç genel düğüm var (kullanıcının dev kit'i). Eski düğümler bir kez USB ile yeniden yüklenir. Geriye uyumluluk (iki çerçeve boyutu, dönüşümlü işaret) hem kodu hem hata yüzeyini büyütür; kullanıcı sayısı artmadan yapmak en ucuzu.

- **Çerçeve:** genel düğüm bağlantıları `NowFrame` kullanır: `NowMsg` başlığı (10 bayt) + 190 bayt yük = **200 bayt**. Mevcut mesajlar (SET, QUERY, STATE, CFG, FORGET) yükü sıfırla doldurur. Eski masa ve yatak düğümleri (10 bayt) etkilenmez.
- **Yeni mesaj türleri:**
  - `NOW_INFO` (sor/yanıt): düğüm sürümü, çip, boş yuva boyutu, rol.
  - `NOW_OTA_BEGIN`: toplam boyut, SHA-256, parça boyutu.
  - `NOW_OTA_DATA`: ofset (4 bayt), uzunluk, veri.
  - `NOW_OTA_ACK`: "şu ofsete kadar aldım" + durum (tamam, hata, bozuk).
  - `NOW_OTA_END` ve `NOW_OTA_ABORT`.
- **Akış denetimi:** köprü 8 parça yollar, sonra düğümün ACK'ını bekler. Düğüm sırayla gelmeyen parçada NAK (beklenen ofset) yollar, köprü geri sarar. SpanPoint `send()` zaten MAC onayı bekliyor; ek olarak uygulama düzeyi onay veriyi güvenceye alır.
- **Süre tahmini:** 1,2 MB ÷ 180 B ≈ 6.700 çerçeve; çerçeve başına ~8–15 ms ile **~1–2 dk** (iyi sinyalde), kötü sinyalde 4–5 dk. Gerçek değer akşam ölçülecek.
- **Servo ve PIR için hazırlık (aynı protokol sürümünde, ikinci kırılmayı önlemek için):**
  - `HELLO`'ya **rol yetenekleri** baytı (röle, servo, PIR...) ve `ACCEPT` içindeki mevcut `reserved` alanına **seçilen rol** baytı.
  - Servo: `NOW_SET` içindeki `val` (0–180) açı olarak kullanılır. PIR: düğüm `NOW_EVENT` (hareket var/yok) yollar, köprü bunu kapı düğümündeki otomasyon motoruna bağlar.
  - OTA mekanizması rolden bağımsızdır, her düğüm türünde aynı çalışır.
  - **Kapı düğümü, isteğe bağlı 2. PIR (kullanıcı fikri, 2026-11-17):** tek PIR varsayılan kalır; ayarda 2. PIR pini açılırsa "girdi" ve "çıktı" tetikleyicileri çıkar (dış PIR → iç PIR = girdi, ters = çıktı, 2–3 sn pencere). Not: PIR hareketsiz kişiyi görmez, görüş alanları daraltılmalı (boru/bant), yön tespiti "çoğunlukla doğru"dur. Daha sağlam girişler aynı yapıya eklenebilir: kapı manyetik sensörü (reed) ve mmWave radar (LD2410, durağan kişiyi de görür). Kural motoruna yeni tetikleyiciler: hareket, girdi, çıktı, kapı açıldı/kapandı.

## 5. Bileşenlere göre iş listesi

**Düğüm (`firmware/node`)**
- Bölüm tablosu: bütün ortamlar `min_spiffs.csv` (C3/C6 sığsın).
- `NowFrame` çerçevesi, `NOW_INFO` yanıtı, OTA alıcı durum makinesi (`esp_ota_begin/write/end`, SHA-256 `mbedtls`).
- Sürüm sabiti (`FW_VERSION`), derlemede `build_web.sh` ile gömülür.
- OTA sırasında röle durumu korunur; yeniden başlayınca mevcut davranış (kapalı başlar).

**Köprü (`firmware/bridge`)**
- Dinamik bağlantılar `NowFrame` boyutunda. `now_link`'e OTA gönderici (HTTP Range ile blok okuma, pencereli gönderme, geri sarma).
- API: `GET /api/node/version?slot=`, `POST /api/node/update?slot=`, `GET /api/node/update` (durum, yüzde, hata). Aynı anda tek güncelleme.
- Güncelleme sırasında köprü o düğümün durumunu sormayı durdurur.

**Site ve derleme**
- `build_web.sh`: düğüm ortamlarının uygulama imajını (`firmware.bin`) `esp32xx.ota.bin` olarak, boyut ve SHA-256 ile `esp32xx.ota.json` olarak yayınlar (USB için `factory.bin` aynen kalır).

**Uygulama (iOS)**
- Cihaz düzenleme ekranında **Yazılım** bölümü: sürüm, "Güncelle", ilerleme çubuğu, "Güncel" durumu.
- Cihazlarım listesinde "güncelleme var" rozeti (denetim kullanıcı isteğiyle, otomatik güncelleme yok).
- İsteğe bağlı sonraki adım: "Tüm düğümleri güncelle" (sırayla).

**CI ve testler (donanımsız)**
- Ana makinede (host) parça bölme, SHA-256 ve ACK/geri sarma mantığı için birim testi.
- CI'da 5 düğüm ortamı + köprü derlemesi, bölüm boyutu denetimi (imaj yuvaya sığıyor mu).

## 6. Aşamalar ve akşam test planı

| Aşama | İçerik | Akşam testi |
|---|---|---|
| 0 | Birim testler, derleme, bölüm boyutu denetimi (**evden bağımsız, şimdi yapılabilir**) | CI yeşil |
| 1 | `min_spiffs` + `NowFrame` + `NOW_INFO`, yeni sürüm yükleme, yeniden eşleşme | Uygulamada düğüm sürümü görünüyor mu, ışık açılıp kapanıyor mu |
| 2 | OTA aktarımı: aynı sürümü kendi üstüne yazma (güvenli deneme) | İlerleme, SHA doğrulama, yeniden başlama |
| 3 | Gerçek güncelleme: siteye 0.3.1 yayınla, uygulamadan "Güncelle" | Sürüm 0.3.0→0.3.1 |
| 4 | Hata denemeleri | Aktarım sırasında düğümü çek, köprüyü çek, manifest SHA'sını boz, uzak mesafe |
| 5 | Uygulama cilası, "Tümünü güncelle" (isteğe bağlı) | |

Akşam için gerekenler:
- **Köprü olarak dev kit'i** genel köprü olarak kur (kişisel köprü OTA yuvasız olduğundan düğüm OTA'sını yapamaz). Ya da kişisel köprüyü de `min_spiffs`'e geçirip bir kez USB ile yüklemek (yazılım sığıyorsa).
- **Bir genel düğüm:** S3 Super Mini. Mümkünse C3 ya da C6 da (yuva boyutu sorunu burada görülür).
- USB kablosu (ilk yükleme ve olası kurtarma için), kartı bilgisayara takabileceğin bir yer.

## 7. Riskler

1. **Bozuk yazılımda geri alma yok:** doğrulama başarısızsa yuva değiştirilmez; yine de çalışmayan bir imaj sağlam bir SHA ile gelirse düğüm USB ile kurtarılır. Önlem: her sürümü yayınlamadan önce bir kartta deneme.
2. **Köprü RAM'i:** HTTPS (mbedTLS) + bağlantı tamponları. Köprü zaten bunu kendi OTA'sı için yapıyor; düğüm aktarımı sırasında başka iş yapmaması sağlanır.
3. **Kötü sinyal:** süre uzar, geri sarma çalışır; kopmada düğüm eski yazılımla devam eder (yuva değişmediği için).
4. **Güncelleme sırasında elektrik kesilmesi:** yuva değişmediği sürece düğüm eski yazılımla açılır.
5. **Çok düğüm:** tek seferde tek güncelleme (hava trafiği ve RAM nedeniyle).

## 8. Kullanıcı kararları

1. **Kırıcı protokol sürümü (v3) şimdi mi?** Önerim evet; mevcut genel düğümler bir kez USB ile yeniden yüklenir ve yeniden eşleşir.
2. **Rol baytı ve servo/PIR hazırlığı aynı sürümde mi?** Önerim evet (ikinci kırılmayı önler); servo/PIR kodu henüz yazılmaz, yalnızca alanlar ayrılır.
3. **Kişisel köprü:** düğüm OTA'sı testi için dev kit'i genel köprü yapmak yeterli mi, yoksa kişisel köprü de `min_spiffs`'e mi geçsin?

## 9. 0. aşama: hazır olanlar (2026-11-17)

| Dosya | Ne |
|---|---|
| `firmware/shared/ota_xfer.h` | Platformdan bağımsız aktarım mantığı: gönderen (pencere, onay, geri sarma, zaman aşımı) ve alan (sıra, yinelenen, boşluk, geçersiz) |
| `firmware/tools/test_ota_xfer.c` | Kayıp, çift ve sırasız teslim eden sahte hatta 12 senaryo (1 bayt, parça sınırları, gerçek 1,3 MB boyut, %30 kayıp...). Çalıştır: `c++ -std=c++17 -x c++ -Ifirmware/shared firmware/tools/test_ota_xfer.c -o /tmp/tox && /tmp/tox` |
| `firmware/tools/check_app_fit.py` | `partitions.bin` + `firmware.bin`: imaj OTA yuvasına sığıyor mu |
| `firmware/tools/make_ota_manifest.py` | `esp32xx.ota.json` (sürüm, boyut, SHA-256); 3. aşamada `build_web.sh`'e bağlanacak |
| `firmware/tools/test_ota_tools.py` | Yukarıdaki iki araç için testler |
| `.github/workflows/ci.yml` | `ota-logic-test` işi + her düğüm derlemesinde sığma denetimi (uyarı) |

**Testin yakaladığı hata:** ilk yazımda kayıp, çift ve sırasız teslim birlikte olunca aktarım **kilitleniyordu** (eski bir NAK göndereni geri sarıyor, sonra gelen yeni onay `acked`'ı `next`'in önüne geçirip pencere hesabını taşırıyordu). Düzeltildi; bu hata donanımda aylarca "bazen takılıyor" diye görünebilirdi.

**Denetimin bulgusu (gerçek derleme):**
```
esp32s3-supermini  1.209.024 B  yuva 1.310.720 B  ok
esp32dev           1.232.992 B  yuva 1.310.720 B  ok
esp32s2            1.220.656 B  yuva 1.310.720 B  ok
esp32c3            1.333.472 B  yuva 1.310.720 B  SIĞMIYOR (22.752 B fazla)
esp32c6            1.340.880 B  yuva 1.310.720 B  SIĞMIYOR (30.160 B fazla)
```
Bu yüzden 1. aşamada tüm düğüm ortamları `min_spiffs.csv`'ye geçecek ve CI'daki `--warn-only` kalkacak.

**Ayarlanacak nokta (2. aşamada ölçülecek):** gönderen şimdi "geri sarma (go-back-N)" yapıyor: bir parça kaybolunca pencerenin tamamı yeniden gidiyor (%10 kayıpta 300 KB'lık aktarımda ~1.000 parça yeniden). Gerçek hatta kayıp düşük olacağı için sorun olmayabilir; 2. aşamada ölçüp gerekirse seçici yeniden gönderme (yalnızca eksik parça) eklenir.
