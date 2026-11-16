# Nowlight animasyon videosu: "Sistem nasıl çalışır" (yapay zekâ istemleri)

**Süre:** ~40 sn. **Format:** 16:9 (sosyal medya için aynı sahneleri 9:16 de üretebilirsin). **Dil:** Türkçe seslendirme, ekran yazıları montajda eklenir.
**Konu:** Telefondan bir ışığa basınca ne olur? Telefon → köprü → ışık düğümü. Sonra yeni cihaz ekleme ve Siri.

> Canlı çekim videosundan ayrı, ona eşlik eden **2D/3D çizgi film tarzı** bir anlatım. Yapay zekâ modelleri yazıyı bozduğu için **hiçbir sahnede yazı ürettirme**; tüm yazılar montajda eklenir. Telefon ekranı gerekiyorsa gerçek görseli ver: [`docs/video/gorseller/01-isiklar.png`](video/gorseller/01-isiklar.png).

## 1. Stil kuralları (tüm istemlerin sonuna ekle)

```
stylized 3D animation, soft isometric cutaway of a cozy house, warm amber and deep navy color palette, glowing golden light, clean minimal shapes, soft shadows, smooth slow camera, Pixar-like polish, no text, no letters, no logos, no captions, 16:9
```

Tutarlılık için **aynı sabit tasarımları** her istemde tarif et:
- **Ev:** küçük, kesit (içi görünen), üç oda: oturma odası (zemin lambası), yatak odası (komodin lambası), mutfak (sarkıt lamba). Gece, dışarısı lacivert.
- **Telefon:** koyu çerçeveli, ekranı kareli sarı/siyah arayüz (verdiğin görsel).
- **Köprü:** küçük yeşil bir devre kartı, yanında beyaz bir router; üstünde yeşil bir ışık yanıp söner.
- **Işık düğümü:** her lambanın altında minik yeşil bir kart.
- **Sinyal:** altın sarısı küçük noktalar ve halkalar (hep aynı renk).

## 2. Başlangıç kareleri (önce bunları üret, sonra sahnelerin ilk karesi yap)

Görsel üretim modelinde (Gemini/Higgsfield) üret, beğenmediğini yenile:

- **A1 Ev, karanlık:** `Isometric cutaway of a small cozy house at night with three rooms: living room with a floor lamp, bedroom with a bedside lamp, kitchen with a pendant lamp. All lamps are off, deep navy tones, soft moonlight through the windows. ` + stil satırı
- **A2 Ev, ışıklar yanık:** aynı istem, `all three lamps glowing warm amber, cozy and inviting` ekle.
- **A3 Telefon yakın plan:** `A hand holding a modern smartphone in front of the dark cutaway house, the screen shows a dark app with a large yellow toggle (use reference image), warm light from the screen on the fingers.` Referans olarak `01-isiklar.png`'yi ver; ekran içeriğinin değişmemesini iste.
- **A4 Köprü:** `Close-up of a small green microcontroller board standing next to a white Wi-Fi router on a shelf in the house, a green LED glows, tiny golden dots drift upward from it.`
- **A5 Yeni düğüm:** `A new small green microcontroller board lying on a table in front of a brass floor lamp, a glowing ring of golden light surrounds it as if it is waking up.`

## 3. Sahneler

Her sahne **görselden videoya** üretilir (başlangıç karesi yazılan görsel). Süreler yaklaşıktır; 5-8 sn'lik klipler üret ve montajda kes.

### Sahne 1: Gece, karanlık ev (5 sn)
Başlangıç karesi: A1.
```
Slow gentle push-in on the dark cutaway house at night, moonlight, the three lamps are off, a quiet peaceful mood, tiny dust particles floating in the air.
```
+ stil satırı
Seslendirme: "Akşam oldu, evin karanlık. Işıkları açmak için kalkmana gerek yok."

### Sahne 2: Telefonda dokunuş (5 sn)
Başlangıç karesi: A3.
```
A thumb taps the yellow toggle on the phone, the phone screen content stays exactly as in the reference image, sharp and legible, no new text, a soft golden ring pulses outward from the phone, shallow depth of field.
```
+ stil satırı
Seslendirme: "Telefonda bir düğmeye bas."

### Sahne 3: Sinyal köprüye gider (7 sn)
Başlangıç karesi: A4 (telefon sağda küçük görünsün).
```
A golden pulse travels as a thin glowing line from the phone through the air to the small green bridge board next to the router, the bridge's green LED flashes once, calm precise motion, the camera glides sideways following the pulse.
```
+ stil satırı
Seslendirme: "Telefon, WiFi üzerinden evdeki küçük bir köprüyle konuşur."
Montajda yazı: "Telefon → WiFi → Köprü"

### Sahne 4: Köprü ışıklara dağıtır (8 sn)
Başlangıç karesi: A1 (ev tüm planı).
```
From the small bridge board, golden dots fan out through the air into the three rooms, each dot reaches a small green node under a lamp, and the lamps turn on one after another: living room floor lamp, bedroom bedside lamp, kitchen pendant lamp, each switching on with a warm amber glow, camera slowly pulls back to reveal the whole glowing house.
```
+ stil satırı
Seslendirme: "Köprü komutu ışıklardaki küçük kartlara iletir. Router'a bile ihtiyaç duymadan, kendi radyo bağlantısıyla."
Montajda yazı: "ESP-NOW"

### Sahne 5: Yeni cihaz eklemek (7 sn)
Başlangıç karesi: A5.
```
A new small green microcontroller board wakes up with a soft pulse of golden light, a thin golden line reaches it from the bridge in the background, a ring closes around it like a handshake, then the brass floor lamp next to it lights up.
```
+ stil satırı
Seslendirme: "Yeni bir ışık eklemek mi istiyorsun? Boş bir kartı tak, uygulama onu kendisi bulur."
Montajda yazı: "Takıyorsun. Uygulama buluyor."

### Sahne 6: Siri (6 sn)
Başlangıç karesi: A1.
```
A person's silhouette stands in the dark living room and speaks, soft golden sound waves flow from them toward the lamps, all the lamps glow on at once, warm cozy atmosphere, a phone lies locked on the table in the foreground with a dark screen.
```
+ stil satırı
Seslendirme: "Ya da sesinle yönet. Telefon kilitliyken bile."
Montajda yazı: "Siri ile, kilitliyken de"

### Sahne 7: Kapanış (4 sn)
Başlangıç karesi: A2.
```
The whole cutaway house glowing warm amber in the dark night, slow pull-back until the house floats in a soft navy gradient, small golden stars appear around, empty space on the right side for text.
```
+ stil satırı
Seslendirme: "Nowlight. Açık kaynak, kendi ışıkların için."
Montajda yazı: "Nowlight · flash.matestudios.online · GitHub"

## 4. Üretim ve montaj

1. A1–A5 görsellerini üret, tutarlı olanları seç.
2. Her sahne için video istemini 2–3 kez çalıştır, en temiz olanı seç.
3. **Telefon ekranı** (sahne 2): ekran bozuluyorsa, istemde telefonu karanlık bırak, sonra montajda `01-isiklar.png`'yi ekran alanına yerleştir.
4. **Yazılar:** hepsini montaj programında ekle (CapCut/DaVinci/iMovie). Sahne başına ≤ 5 kelime, büyük ve okunaklı.
5. **Ses:** Seslendirme metinleri yukarıda, toplam ~40 sn'ye sığar. Altına çok hafif, sıcak bir müzik koy; ışık yanarken ufak "tık" ya da "çın" efekti ekle.
6. **Dikey sürüm (9:16):** aynı istemleri `9:16` yazarak yeniden üret; sosyal medya için sahne 2, 4 ve 6'yı birleştir (~20 sn).

## 5. Sorun olursa
- **Lambalar sırayla yanmıyor, hepsi birden yanıyor:** istemde "one after another, with half a second between each" ifadesini güçlendir ya da sahneyi her lamba için ayrı klip üretip montajda birleştir.
- **Evin odaları sahneler arası değişiyor:** her sahnede A1/A2 karesini başlangıç karesi olarak ver ve "same house, same layout as the reference" yaz.
- **Ekrana yazı çıkıyor:** `no text, no letters` ifadesini istemin başına da koy.
