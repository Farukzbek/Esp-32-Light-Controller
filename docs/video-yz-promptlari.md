# Nowlight tanıtım videosu: yapay zekâ istemleri ve görseller

Bu belge, videoyu **tamamen yapay zekâ ile** (Higgsfield, Gemini/Veo, Kling gibi) üretmek için hazır istemleri ve verilecek görselleri içerir. Senaryo ve seslendirme metni için bkz. [video-senaryosu.md](video-senaryosu.md).

Video, sistemi **simüle eder**: gerçek çekim yok. Uygulama ekranları gerçek uygulamadan alınmış temiz ekran görüntüleri; donanım (kart, lamba, router) yapay zekâ ile üretilir.

## 1. Görseller (yapay zekâya verilecekler)

Klasör: [`docs/video/gorseller/`](video/gorseller/). Hepsi iPhone 17 Pro çözünürlüğünde (1206×2622), saat 9:41, pil dolu, örnek veriyle (Salon, Yatak odası, Mutfak).

| Dosya | İçerik | Hangi sahnede |
|---|---|---|
| `01-isiklar.png` | Ana ekran: Tüm ışıklar, Salon açık, Siri'ye ekle kartı | Açılış, kapanış, "ışık artık ana ekranında" |
| `02-widgetlar.png` | Küçük/orta/büyük widget önizlemeleri | Widget anı |
| `03-cihaz-ekle.png` | Cihaz Ekle sekmesi, Cihazlarım listesi | Eşleşmeye giriş |
| `04-eslesme-hazirlik.png` | "Yeni cihazı hazırla" (3 adım) | Eşleşme başı |
| `05-eslesme-araniyor.png` | "Cihaz aranıyor" | Arama anı |
| `06-eslesme-bulundu.png` | "Cihaz bulundu: Nowlight-1122" | Cihaz çıkınca |
| `07-eslesme-rol.png` | "Bu cihaz ne olacak?" | Rol seçimi |
| `08-eslesme-ad-simge.png` | Ad "Koridor", simge seçili, Bağla | Ad ve simge |
| `09-siri-ekle-liste.png` | Siri'ye ekle: ışık listesi | Siri bölümü |
| `10-siri-ekle-secili.png` | Üç ışık seçili, "Seçilenleri ekle (6)" | Siri bölümü |
| `11-web-yukleyici.png` | flash.matestudios.online (masaüstü, 1280×900) | Yazılım yükleme |

> Eksik olan ve yapay zekâyla üretilecek kareler: kart, röle, lamba, router. Bunlar için aşağıda **referans görsel istemleri** var.

## 2. Yapay zekâ videosunun en önemli kuralı

Yapay zekâ modelleri, verdiğin ekranın **yazısını bozar** (harfler kayar, düğmeler uydurulur). Bunu azaltmak için:

1. **Ekran sahnelerini kısa tut** (en fazla 4–5 sn) ve istemde "ekran içeriği değişmesin" de.
2. **Hareketi sade tut:** yavaş yaklaşma (push-in) ve bir parmak dokunuşu. Ekranda animasyon, kaydırma ya da yeni metin isteme.
3. **Yazının okunması gereken sahneleri** (web yükleyici, rol seçimi, ad/simge) yapay zekâya yaptırma. Bunları görseli olduğu gibi montaj programına koy, hafif yakınlaştırma (Ken Burns) ver. Yapay zekâyı yalnızca el, parmak ve telefon çerçevesi için kullan.
4. Her sahneyi 2–3 kez üret, en temiz olanı seç.
5. Telefonu **elde** gösteren sahnelerde en güvenli yol: görseli montajda bir telefon çerçevesine yerleştirip, arka planı yapay zekâ videosundan almak.

## 3. Ortak stil satırı (her istemin sonuna ekle)

```
cinematic product video, warm cozy indoor lighting, shallow depth of field, soft film grain, smooth camera motion, 16:9, no text overlays, no logos, no captions
```

Ekran (görselli) sahnelerde bunun yerine şunu kullan:

```
the phone screen content must stay exactly as in the reference image, sharp and legible, no new text, no changes to the interface, minimal camera motion, 9:16 vertical
```

## 4. Referans görsel istemleri (donanım, tek seferlik; bunları üretip tüm sahnelerde başlangıç karesi olarak kullan)

Tutarlılık için önce bu dört kareyi üret (Gemini/Higgsfield görsel üretimi), sonra videolarda başlangıç karesi yap.

- **R1 ESP32 kartı:** `Macro photo of a small green ESP32 microcontroller development board lying on a light wooden desk, USB-C port visible, soft window light, shallow depth of field, photorealistic, no text on the board.`
- **R2 Röle modülü:** `Photo of a small blue single-channel relay module next to jumper wires on a clean workbench, soft daylight, photorealistic, no readable text.`
- **R3 Oturma odası ve lamba:** `A cozy living room in the evening, a floor lamp with a warm fabric shade next to a sofa, the lamp is off, calm and tidy, photorealistic, wide shot.`
- **R4 Router ve küçük kart:** `A white Wi-Fi router on a bookshelf with softly blinking green lights, a small microcontroller board standing next to it, cozy living room background, photorealistic.`

Aynı odayı ve lambayı her sahnede kullan: istemlere hep "the same cozy living room with the same floor lamp" ekle.

## 5. Sahneler

Süreler toplam videoyu ~2:30 (köprü) + ~3:30 (ışık ekleme) yapar. Her satırda: **görsel**, **istem (İngilizce)**, **seslendirme (Türkçe)**.

### Bölüm 1: Köprüyü kur

**K1: Açılış (6 sn).** Görsel: R4.
İstem: `Slow push-in on a white Wi-Fi router with softly blinking lights on a bookshelf, then the camera glides to a small green microcontroller board standing next to it. The same cozy living room in the evening. 6 seconds.` + ortak stil.
Seslendirme: "Nowlight'ın bir kez kurulan bir köprüsü var. Telefonun ışıklarla bu köprü üzerinden konuşur. Önce onu kuralım."

**K2: Web yükleyici (5 sn).** Görsel: `11-web-yukleyici.png`. Yapay zekâ kullanma; montajda yavaş yakınlaştır, "Ne kuruyorsun?" bölümüne odaklan.
Seslendirme: "flash.matestudios.online sayfasını Chrome ya da Edge ile aç. Ne kuruyorsun kısmından Köprü'yü seç, sonra kendi kartını."

**K3: USB'ye takma (6 sn).** Görsel: R1.
İstem: `A hand plugs a USB-C cable into a small green ESP32 development board on a wooden desk next to a laptop, a tiny LED on the board lights up, calm and precise movement. The same desk, soft daylight. 6 seconds.` + ortak stil.
Seslendirme: "Kartı veri kablosuyla tak, Yükle'ye bas ve portu seç. Bir dakika sürüyor."

**K4: WiFi ayarı (4 sn).** Montajda grafik: bilgisayar ekranında "Ağını seç" kartı (Canva/Keynote ile düz tasarım; yapay zekâyla üretme, yazı bozulur). Şifre alanını noktalı göster.
Seslendirme: "Yükleme bitince sayfa WiFi bilgisini sorar. Ağını seç, şifreni yaz. Şifre sadece köprünün hafızasına gider, başka hiçbir yere yollanmaz."

**K5: Uygulamada bul (5 sn).** Görsel: `03-cihaz-ekle.png` (telefon çerçevesinde, hafif yaklaşma). Yapay zekâ ile yalnızca telefonu tutan el ve çerçeve.
İstem: `A hand holds a smartphone in a dim living room, the screen content must stay exactly as in the reference image, sharp and legible, no new text, no changes to the interface, a thumb taps near the top of the screen, minimal camera motion, 9:16 vertical. 5 seconds.`
Seslendirme: "Telefonda Köprü ayarları'na gir ve Köprüyü otomatik bul'a bas. Adres ve anahtar kendiliğinden dolar. Kaydet de, köprü hazır."

**K6: Köprü adaptörde (5 sn).** Görsel: R4.
İstem: `A small green microcontroller board plugged into a white USB wall charger on a bookshelf, subtle LED glow, calm atmosphere, the same cozy living room. 5 seconds.` + ortak stil.
Seslendirme: "Köprüyü bir adaptöre tak, evde bir köşede dursun. Şimdi ışıkları ekleyelim."

### Bölüm 2: Işık düğümü ekle

**1: Açılış (6 sn).** Görsel: R1, sonra R3.
İstem: `Close-up of a small green ESP32 microcontroller board on a wooden desk, slow dolly in, then cut to the same cozy living room where a floor lamp turns on with a soft warm glow. 6 seconds.` + ortak stil.
Seslendirme: "Elinde boş bir ESP32 var. Beş dakikada onu telefonundan açıp kapatabildiğin bir lambaya çevireceğiz. Kod yazmana, program kurmana gerek yok."

**2: Malzemeler (4 sn).** Grafik kartı (montaj programında): ESP32, röle, lamba, kablo. Güvenlik uyarısı kutusu.
Seslendirme: "Bir ESP32, bir röle modülü ve lamba yeterli. Şebeke gerilimiyle çalışacaksan, bağlantıyı elektrik bilen biriyle yap, sigortayı kapat. İlk denemeyi sadece düşük gerilimle yapmanı öneririm."

**3: Kart seçimi (5 sn).** Görsel: `11-web-yukleyici.png`, montajda yakınlaştır.
Seslendirme: "Bilgisayarında Chrome ya da Edge ile flash.matestudios.online sayfasını aç. Listeden kendi kartını seç. Listede yoksa aynı çipten herhangi birini seç."

**4: Yükleme (5 sn).** Görsel: R1 + montajda ilerleme çubuğu grafiği.
İstem: `A green microcontroller board connected by a USB cable to a laptop, a small status LED blinking, calm workbench, soft daylight, slow push-in. 5 seconds.` + ortak stil.
Seslendirme: "Kartı veri kablosuyla tak. Yükle'ye bas, çıkan listeden kartının portunu seç. Bir dakika sürüyor. Port görünmezse BOOT tuşuna basılı tutarak kabloyu tak."

**5: Röle bağlantısı (6 sn).** Görsel: R2 + R1. Bağlantı şemasını (GPIO5 → IN, 5V, GND) montajda **kendi çizdiğin düz grafik** olarak ekle: yapay zekâ kabloları yanlış çizer.
İstem: `Overhead shot of hands connecting jumper wires between a small microcontroller board and a relay module on a clean workbench, calm and precise movements, soft daylight. 6 seconds.` + ortak stil.
Seslendirme: "Rölenin sinyal ucunu GPIO5'e, güç uçlarını 5 volta ve GND'ye bağla. Kartı bir USB adaptörüne tak. Başka bir şey ayarlamana gerek yok."

**6: Cihaz bulunur (8 sn).** Görseller sırayla: `04-eslesme-hazirlik.png`, `05-eslesme-araniyor.png`, `06-eslesme-bulundu.png`. Ekran sahneleri; montajda kısa geçişlerle birleştir.
İstem (ekran sahneleri için, görsel başına): `the phone screen content must stay exactly as in the reference image, sharp and legible, no new text, no changes to the interface, a thumb taps the yellow button, minimal camera motion, 9:16 vertical. 3 seconds.`
Seslendirme: "Uygulamada Cihaz Ekle'ye gir, Cihaz ekle'ye bas. Kartın birkaç saniye içinde ekranda çıkar. Üzerine dokun."

**7: Rol, ad, simge (8 sn).** Görseller: `07-eslesme-rol.png`, `08-eslesme-ad-simge.png`. Yazı okunması gereken sahne: yapay zekâ kullanma, montajda yakınlaştır, dokunma anı için küçük bir parmak/halka animasyonu ekle.
Seslendirme: "Yeni ışık olarak ekle de, bir ad ver, bir simge seç. Bağla'ya bas."

**8: Lamba yanar (6 sn).** Görsel: R3 ve `01-isiklar.png`.
İstem: `A floor lamp in the same cozy living room turns on with a gentle warm glow, a hand holding a smartphone enters the frame from the side but the phone screen is not visible. 6 seconds.` + ortak stil. Ardından montajda `01-isiklar.png` (Salon açık) ve `02-widgetlar.png` göster.
Seslendirme: "Aç ve kapat düğmelerini dene. Lamba çalışıyorsa evet de. Hepsi bu. Işık artık ana ekranında, widget'larında ve Siri'de."

**8b: Siri (8 sn).** Görseller: `09-siri-ekle-liste.png`, `10-siri-ekle-secili.png`. Sesli komut için montajda ses dalgası grafiği.
Seslendirme: "İstersen ışıklarını sesinle de yönet: Siri'ye ekle'den ışıkları seç, 'salon kapat' de, telefon kilitliyken bile çalışır."

**9: Sorun giderme (4 sn).** Grafik kartı (montaj): BOOT tuşu 5 sn = eşleşmeyi sil; "Cihaz bulunamadı" için ipuçları.
Seslendirme: "Bir şey olmazsa: kartın BOOT tuşuna beş saniye basıp eşleşmeyi silebilirsin. Köprüyle aynı WiFi'de olduğundan emin ol."

**10: Kapanış (6 sn).** Görsel: R3.
İstem: `Slow pan across the same cozy living room with several lamps glowing warm yellow at dusk, peaceful atmosphere. 6 seconds.` + ortak stil. Üstüne montajda: flash.matestudios.online ve GitHub adresi.
Seslendirme: "Daha fazlası ve açık kaynak kodu GitHub'da. İyi aydınlıklar."

## 6. Üretim sırası

1. R1–R4 referans görsellerini üret ve beğenmediğini yenile.
2. Kart, el ve lamba sahnelerini (K1, K3, K6, 1, 4, 5, 8, 10) görsel→video modunda üret.
3. Ekran sahnelerinde (K5, 6, 7, 8b) önce yalnızca montajda görsel + yakınlaştırma dene. Yetmezse "ekran içeriği değişmesin" istemiyle 3 sn'lik parmak dokunuşu üret.
4. Seslendirmeyi bu belgedeki metinlerden üret (Gemini ses üretimi, ElevenLabs vb.; hız hafif yavaş).
5. Montaj: CapCut, DaVinci Resolve ya da iMovie. Kliplerin arasına 0,3 sn'lik yumuşak geçiş, altta çok hafif müzik.

## 7. Yeniden görsel almak istersen

Uygulama değişince ekranları yeniden almak için: simülatörde uygulamayı sahte köprüyle aç (`python3 ios/tools/mock_bridge.py 8080 test`; başlatma argümanı `-bridgeHost 127.0.0.1:8080 -bridgeToken test`), durum çubuğunu `xcrun simctl status_bar booted override --time 9:41 --batteryState charged --batteryLevel 100` ile sabitle ve `xcrun simctl io booted screenshot dosya.png` ile kaydet. Ad ve simgeler `defaults write group.online.matestudios.nowlight deviceNames -dict …` ile verilir.
