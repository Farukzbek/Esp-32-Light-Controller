# Nowlight: iOS uygulaması ve widget'lar

Evdeyken (aynı WiFi'de) telefondan ışıkları kontrol eder. Telefon, [`firmware/bridge`](../firmware/bridge) köprüsüne HTTP ile bağlanır; köprü ESP-NOW ile düğümlerle konuşur.

- **Uygulama (alt sekmeler):**
  - **Işıklar:** yalnızca ışık düğmeleri (simgeli kartlar, panelle aynı renkler, açıkken sıcak parıltı, dokununca titreşim, aşağı çekerek yenileme). Bağlantı sorunu olursa üstte kısa uyarı çıkar. "Tüm ışıklar" kartı yalnızca hepsi açıkken tamamen yanar, bir kısmı açıkken "1/3" yazar.
  - **Widgetlar:** üç boyutun (küçük, orta, büyük) gerçek durumla önizlemesi ve ana ekrana nasıl ekleneceği.
  - **Cihaz Ekle:** cihaz yönetimi. "Cihazlarım" listesinde bir cihazı sola kaydırıp **kaldırabilirsin** (yalnızca uygulamadan ve widget'lardan kalkar, ESP32 çalışmaya devam eder), "Kaldırılanlar"dan **geri ekleyebilirsin**. Köprü ayarları (adres, token) da buradan açılır. **Otomatik cihaz arama/kurulum (henüz hiçbir köprüye bağlanmamış bir ESP32'yi kod girmeden kurma) sonraki sürümde gelecek**; ekranda "Yakında" olarak duruyor.
  - Uygulama simgesi: `App/Assets.xcassets`.
- **Widget (küçük, orta, büyük):** Widget'a uzun basıp **Widget'ı Düzenle** ile hangi ışıkların (en fazla 4, sırasıyla; "Boş" seçilen yuva gösterilmez) ve açık renginin (sarı, yeşil, mavi, beyaz) görüneceğini seçersin.
  - Küçük: 2x2 küçük karolar. Orta: tek satırda karolar, durum yazısıyla. Büyük: başlık, büyük karolar ve "Hepsini aç / Hepsini kapat".
  - iOS 17 etkileşimli düğmeleri: basınca uygulamayı açmadan ışık değişir. Köprüye ulaşılamazsa son bilinen durum "Güncel değil" uyarısıyla soluk gösterilir.
- **Kısayollar/Siri:** `ToggleLightIntent` ve `SetAllLightsIntent` Kısayollar uygulamasında görünür.

## Derleme
Gerekenler: Xcode 16+, [XcodeGen](https://github.com/yonaskolb/XcodeGen) (`brew install xcodegen`), iOS 17+ cihaz.

```bash
cd ios/Nowlight
xcodegen generate
open Nowlight.xcodeproj
```
Xcode'da iki hedefte (Nowlight, NowlightWidget) **Signing & Capabilities → Team** seç. Kimlikler `project.yml`'de (`online.matestudios.nowlight`, App Group `group.online.matestudios.nowlight`); kendi hesabında farklı bir önek kullanacaksan `project.yml`, `Shared/BridgeConfig.swift` ve iki `.entitlements` dosyasında birlikte değiştir.

İlk açılışta Ayarlar'dan köprü adresini (`nowlight.local` ya da IP) ve köprünün `token` komutuyla gösterdiği anahtarı gir, **Bağlantıyı dene**'ye bas. iOS "yerel ağ" iznini isteyecek, ver.

## Köprü olmadan deneme
```bash
python3 ios/tools/mock_bridge.py 8080 test      # köprüyü taklit eder
xcrun simctl launch booted online.matestudios.nowlight -bridgeHost 127.0.0.1:8080 -bridgeToken test
```

## Geliştirme ipucu
Uygulamayı belirli bir sekmede açmak için `-tab 0|1|2` başlatma argümanı kullanılır (0 Işıklar, 1 Widgetlar, 2 Cihaz Ekle).

## Notlar
- Token ve adres App Group UserDefaults'ta saklanır (uygulama ve widget ortak okur). Yalnızca ev ağında geçerli, düşük riskli bir anahtar için yeterli; internete açılan bir köprüde Keychain kullanılmalı.
- Widget'ın yenilenme sıklığını iOS belirler; düğmeye basınca ve uygulamada işlem yapınca hemen yenilenir.
