# Nowlight: iOS uygulaması ve widget'lar

Evdeyken (aynı WiFi'de) telefondan ışıkları kontrol eder. Telefon, [`firmware/bridge`](../firmware/bridge) köprüsüne HTTP ile bağlanır; köprü ESP-NOW ile düğümlerle konuşur.

- **Uygulama (alt sekmeler):**
  - **Işıklar:** yalnızca ışık düğmeleri (simgeli kartlar, panelle aynı renkler, açıkken sıcak parıltı, dokununca titreşim, aşağı çekerek yenileme). Bağlantı sorunu olursa üstte kısa uyarı çıkar. "Tüm ışıklar" kartı yalnızca hepsi açıkken tamamen yanar, bir kısmı açıkken "1/3" yazar.
  - **Widgetlar:** üç boyutun (küçük, orta, büyük) gerçek durumla önizlemesi ve ana ekrana nasıl ekleneceği.
  - **Cihaz Ekle:** cihaz yönetimi. "Cihazlarım" listesinde bir cihazı sola kaydırıp **kaldırabilirsin** (yalnızca uygulamadan ve widget'lardan kalkar, ESP32 çalışmaya devam eder), "Kaldırılanlar"dan **geri ekleyebilirsin**. Köprü ayarları (adres, token) da buradan açılır. **Otomatik cihaz arama/kurulum (henüz hiçbir köprüye bağlanmamış bir ESP32'yi kod girmeden kurma) sonraki sürümde gelecek**; ekranda "Yakında" olarak duruyor.
  - Uygulama simgesi: `App/Assets.xcassets`.
- **Widget (küçük, orta, büyük):** Widget'a uzun basıp **Widget'ı Düzenle** ile hangi ışıkların (en fazla 8, sırasıyla; "Boş" seçilen yuva gösterilmez) ve açık renginin (sarı, yeşil, mavi, beyaz) görüneceğini seçersin.
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
Xcode'da üç hedefte (Nowlight, NowlightWidget, NowlightIntents) **Signing & Capabilities → Team** seç. Kimlikler `project.yml`'de (`online.matestudios.nowlight`, App Group `group.online.matestudios.nowlight`); kendi hesabında farklı bir önek kullanacaksan `project.yml`, `Shared/BridgeConfig.swift` ve üç `.entitlements` dosyasında birlikte değiştir.

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

## Siri komutları

- **Hazır komutlar:** uygulama kurulunca "Nowlight tüm ışıkları aç/kapat" kendiliğinden gelir (App Shortcuts).
- **Kendi cümlen ("led kapat"):** Işıklar ekranının altındaki **Siri'ye ekle** kartından ışıkları seç; her ışık için "aç" ve "kapat" Siri sayfaları art arda açılır, cümleyi bir kez sesli söylersin (Apple zorunlu tutuyor). Aynı ekleme ışığın düzenleme ekranından da yapılır.
- **Kilitliyken de çalışır:** bu komutları `NowlightIntents` Siri uzantısı (`IntentsExt/`, tanım `Shared/Siri.intentdefinition`) uygulamayı açmadan arka planda çalıştırır.
- Uzantının App Group'u Xcode'da bir kez seçilmelidir (NowlightIntents → Signing & Capabilities → App Groups → `group.online.matestudios.nowlight`); komut satırı derlemesi bunu kendiliğinden yapamaz.
- Not: komutun başına "ışık" gibi kelimeler koyma, Siri bunu Apple Ev komutu sanıp "evinde ayarlanmış ışık yok" der. Sade adlar ("led kapat") güvenli.

## Roller: servo ve hareket sensörü (PIR)

Yeni cihaz eklerken rol seçilir: **Işık**, **Servo** (aç/kapat için iki ayarlı açı + elle açı kaydırıcısı + "Bırak") ya da **Hareket sensörü**. Servo ve PIR "Tüm ışıklar"a katılmaz. Servo, hedefe varınca ayarlı süre sonra bırakılır ve elle oynatılabilir (karşı koymaz). Otomasyon kurallarında servo "Aç" = açık açısına, "Kapat" = kapalı açısına gider. Kurallara **gün seçimi** eklendi (örneğin perde Pazar günleri açılmasın). Ayrıntı: [docs/protocol.md](../docs/protocol.md).

Gelistirme: `tools/mock_bridge.py 8080 test --roles` (servo d3 + PIR d4 ile) ve `tools/mock_door.py` (Otomasyon ekranı için sahte kapı düğümü).

## Demo modu

**Ayarlar → Demo modu**: köprü ve cihaz olmadan uygulamanın her yerini dene. Uygulama içinde çalışan sahte bir köprü ve kapı düğümü (`App/DemoMode.swift`, URLProtocol) örnek ışıklar, bir servo (Perde) ve bir PIR (Kapı sensörü) sunar; cihaz ekleme (4 sn sonra yeni cihaz bulunur), cihaz/servo/PIR ayarları, programlar, kapı otomasyonu ve köprü güncellemesi gerçekmiş gibi çalışır. Ağa hiçbir şey gitmez. Açılırken gerçek ayarların yedeği alınır, kapatılınca geri yüklenir. Widget ve Siri demo cihazlarını görmez (ayrı süreçler).

## Programlar (saat ve gün)

**Cihaz Ekle → Programlar**: "her sabah 07:30'da perdeyi 90° konumuna getir, Pazar hariç" gibi saate ve güne bağlı kurallar. Kurallar telefonda yazılır, köprüye gönderilir (`POST /api/schedules`) ve köprüde çalışır (NTP saati, telefon kapalıyken de). Servolar için "açıya git", diğerleri için aç/kapat/çevir. Plan: [docs/plan-zamanli-otomasyon.md](../docs/plan-zamanli-otomasyon.md).
