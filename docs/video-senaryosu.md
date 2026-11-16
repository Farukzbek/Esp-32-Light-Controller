# Video: Nowlight'ı sıfırdan kurmak (köprü + ışık düğümü)

**Amaç:** ESP'den hiç anlamayan biri, boş ESP32'lerle evini kurup 10 dakikada uygulamadan ışık yakabilsin.
**Süre:** İki bölüm: **Bölüm 1 köprü kurulumu** (~2:30 dk) ve **Bölüm 2 ışık ekleme** (~3:30–4:30 dk). İki ayrı video olarak da yayınlanabilir. **Dil:** Türkçe, sakin ve kısa cümleler. **Format:** 16:9, 1080p; telefon ekranı dikey kutuda ya da yan yana.
**Yayın yeri:** YouTube (liste dışı da olur), `flash.matestudios.online` sayfasına ve README'ye bağlantı.

> Not: Bu bir **taslak senaryo**. Gerçek çekim, köprü ile düğüm eşleşmesi donanımda doğrulandıktan sonra yapılacak. Ekranlar uygulamanın güncel sürümüne göre kaydedilmeli.

## Gerekenler (video başında ekranda listelenir)
- Bir ESP32 kartı (S3 / C3 / C6 / S2 / klasik) ve veri kablosu
- Röle modülü (tercihen ters mantık, 5V) ve bir lamba
- Bölüm 1'deki gibi kurulmuş Nowlight köprüsü (video 2'de zaten kurulu varsayılır)
- Chrome ya da Edge'li bir bilgisayar (veya Android telefon)
- Nowlight uygulaması yüklü iPhone

## Bölüm 1: Köprüyü kur (evde bir kez, ~2:30)

| # | Süre | Ekran / çekim | Seslendirme (taslak) |
|---|---|---|---|
| K1 | 0:00–0:15 | Evin haritası ya da router yakın plan, sonra küçük bir ESP32 kartı. Başlık: "Önce köprü" | "Nowlight'ın bir kez kurulan bir köprüsü var. Telefonun ışıklarla bu köprü üzerinden konuşur. Önce onu kuralım." |
| K2 | 0:15–0:45 | **Gerçek ekran kaydı:** flash.matestudios.online. "Ne kuruyorsun?" listesinde **Köprü** seçilir, kart seçilir. | "flash.matestudios.online sayfasını Chrome ya da Edge ile aç. Ne kuruyorsun kısmından Köprü'yü seç, sonra kendi kartını." |
| K3 | 0:45–1:15 | **Gerçek ekran kaydı:** USB'ye takma, Yükle, port seçme, ilerleme çubuğu. | "Kartı veri kablosuyla tak, Yükle'ye bas ve portu seç. Bir dakika sürüyor." |
| K4 | 1:15–1:50 | **Gerçek ekran kaydı:** WiFi ayarı penceresi, ağ seçilir, şifre girilir (şifre alanı bulanık), "Bağlandı". | "Yükleme bitince sayfa WiFi bilgisini sorar. Ağını seç, şifreni yaz. Şifre sadece köprünün hafızasına gider, başka hiçbir yere yollanmaz." |
| K5 | 1:50–2:20 | **Gerçek ekran kaydı (iPhone):** Cihaz Ekle → Köprü ayarları → Köprüyü otomatik bul, "✓ Köprü bulundu", Kaydet. Adres ve token alanları bulanık. | "Telefonda Köprü ayarları'na gir ve Köprüyü otomatik bul'a bas. Adres ve anahtar kendiliğinden dolar. Kaydet de, köprü hazır." |
| K6 | 2:20–2:30 | Köprü USB adaptöre takılır, kenara konur. | "Köprüyü bir adaptöre tak, evde bir köşede dursun. Şimdi ışıkları ekleyelim." |

> Köprü kurulum penceresi 10 dakika açık kalır. Kapanırsa köprünün BOOT tuşuna 2 saniye basılır (bunu sorun giderme bölümünde söyle).

## Bölüm 2: Işık düğümü ekle (her yeni ışık için)


| # | Süre | Ekran / çekim | Seslendirme (taslak) |
|---|---|---|---|
| 1 | 0:00–0:15 | Boş bir ESP32 kartı yakın plan, sonra kurulmuş lamba. Başlık: "Nowlight: 5 dakikada yeni ışık" | "Elinde boş bir ESP32 var. Beş dakikada onu telefonundan açıp kapatabildiğin bir lambaya çevireceğiz. Kod yazmana, program kurmana gerek yok." |
| 2 | 0:15–0:35 | Malzeme listesi (ESP32, röle, lamba, kablo). Güvenlik uyarısı kutusu. | "Bir ESP32, bir röle modülü ve lamba yeterli. Şebeke gerilimiyle çalışacaksan, bağlantıyı elektrik bilen biriyle yap, sigortayı kapat. İlk denemeyi sadece düşük gerilimle yapmanı öneririm." |
| 3 | 0:35–1:05 | Bilgisayar, tarayıcıda `flash.matestudios.online`. Kart listesi açılır, kart seçilir. | "Bilgisayarında Chrome ya da Edge ile flash.matestudios.online sayfasını aç. Listeden kendi kartını seç: kartın üzerinde ESP32-S3, C3, C6 gibi bir yazı olur. Listede yoksa aynı çipten herhangi birini seç." |
| 4 | 1:05–1:45 | Kartı USB'ye takma. "Yükle" düğmesi, port seçme penceresi, ilerleme çubuğu, "Kurulum tamamlandı". | "Kartı veri kablosuyla tak. Yükle'ye bas, çıkan listeden kartının portunu seç. Bir dakika sürüyor. Port görünmezse BOOT tuşuna basılı tutarak kabloyu tak." |
| 5 | 1:45–2:15 | Röle bağlantısı: ESP32 GPIO5 → röle IN, 5V, GND. Kart bir USB adaptörüne takılır. | "Rölenin sinyal ucunu GPIO5'e, güç uçlarını 5 volta ve GND'ye bağla. Kartı bir USB adaptörüne tak. Başka bir şey ayarlamana gerek yok." |
| 6 | 2:15–3:00 | iPhone ekranı: Cihaz Ekle → Cihaz ekle → Cihaz ara. Kart "Nowlight-XXXX" olarak çıkar, seçilir. | "Uygulamada Cihaz Ekle'ye gir, Cihaz ekle'ye bas. Kartın birkaç saniye içinde ekranda çıkar. Üzerine dokun." |
| 7 | 3:00–3:30 | Rol adımı: "Yeni ışık olarak ekle", ad (Salon), simge seçimi, Bağla. | "Yeni ışık olarak ekle de, bir ad ver, bir simge seç. Bağla'ya bas." |
| 8 | 3:30–4:00 | Deneme adımı: Aç / Kapat, lamba yanıp söner (çekim). "Evet, çalışıyor". Ana ekranda yeni kart. | "Aç ve kapat düğmelerini dene. Lamba çalışıyorsa evet de. Hepsi bu. Işık artık ana ekranında, widget'larında ve Siri'de." |
| 9 | 4:00–4:20 | Sorun giderme kartı: BOOT 5 sn = eşleşmeyi sil; "Cihaz bulunamadı" için ipuçları. | "Bir şey olmazsa: kartın BOOT tuşuna beş saniye basıp eşleşmeyi silebilirsin. Köprüyle aynı WiFi'de olduğundan emin ol." |
| 10 | 4:20–4:30 | Kapanış ekranı: adresler ve GitHub. | "Daha fazlası ve açık kaynak kodu GitHub'da. İyi aydınlıklar." |

## Çekim planı
**Ekran kayıtları**
- Mac: QuickTime → Yeni Ekran Kaydı, tarayıcı penceresini kaydet (sahne 3, 4).
- iPhone: Denetim Merkezi → Ekran Kaydı, Rahatsız Etmeyin açık (sahne 6, 7, 8).
- Uygulama sahnelerinin bir kısmı simülatörde ve sahte köprüyle alınabilir (`ios/tools/mock_bridge.py`); gerçek cihaz sahnesi için telefon kaydı kullan.

**Kamera çekimleri**
- Sahne 1: kart ve lamba yakın plan.
- Sahne 5: bağlantı şeması üstten, eller görünsün.
- Sahne 8: lambanın yanıp sönmesi (telefonda düğmeye basılırken aynı karede).

**Ses:** telefon mikrofonu sessiz odada yeterli; müzik hafif ve altta.

## Teknik notlar (çekimden önce kontrol)
- `flash.matestudios.online` bu ağda DNS önbelleği yüzünden geç açılabilir; çekim öncesi açıldığını kontrol et.
- Köprü yazılımı eşleşme destekli sürümde olmalı (`firmware/bridge`, commit `7388a86` ve sonrası).
- Çekimde gösterilen WiFi adı, token ve IP gibi bilgileri bulanıklaştır.
- Uygulamanın rehber yazıları (Cihaz nasıl eklenir?) yeni akışa güncellenmeli (yapılacaklar #11).

## Çekim sonrası
- Videoyu `flash.matestudios.online` sayfasının altına ve README'ye bağla.
- Kısa bir sürümü (30 sn, sahne 3–8 hızlı) sosyal medya için kes.

---

# Yapay zekâ ile üretim (Higgsfield / Gemini)

Video, kamerayla çekilmeyecek; görüntüler yapay zekâ ile üretilecek. Kural: **yazı ve arayüz içeren her sahne gerçek ekran kaydı olmalı.** Yapay zekâ modelleri ekrandaki metni ve uygulama arayüzünü bozar (harfler değişir, düğmeler uydurulur). Yapay zekâ yalnızca "atmosfer" görüntüleri için kullanılır: kart, lamba, eller.

| Sahne | Kaynak | Not |
|---|---|---|
| K1 Köprü açılışı | YZ video | Router/kart yakın plan |
| K2, K3, K4, K5 Köprü kurulumu | **Gerçek ekran kaydı** | Web sayfası, WiFi ayarı, uygulama |
| K6 Köprü adaptörde | YZ video | Kısa kesit |
| 1 Açılış | YZ video | Kart + yanan lamba |
| 2 Malzemeler | Tasarım (Canva/Keynote) | Düz kart grafikleri; YZ ile üretme, çünkü yazı içerir |
| 3 Web sayfası, kart seçimi | **Gerçek ekran kaydı** | `flash.matestudios.online` |
| 4 Yükleme | **Gerçek ekran kaydı** | Chrome, Yükle düğmesi, ilerleme çubuğu |
| 5 Röle bağlantısı | YZ video + kendi çizimin | Bağlantı şemasını (GPIO5 → IN, 5V, GND) düz grafik olarak ekle; kabloları YZ yanlış çizer |
| 6, 7, 8 Uygulama | **Gerçek ekran kaydı** | iPhone ya da simülatör; sahte köprüyle de alınabilir |
| 8 Lamba yanması | YZ video | Kısa kesit |
| 9 Sorun giderme | Tasarım kartı | Düz metin grafiği |
| 10 Kapanış | Tasarım | Adresler, GitHub |

## YZ video istemleri (İngilizce yaz, modeller daha iyi anlıyor)
Ortak stil, her isteme ekle: `cinematic product video, warm cozy indoor lighting, shallow depth of field, soft film grain, 16:9, no text, no logos, no captions`

0. **Köprü açılışı (K1)** – `Close-up of a Wi-Fi router with softly blinking lights on a shelf in a cozy living room, then a slow push-in toward a small microcontroller board standing next to it. 6 seconds.`
0b. **Köprü adaptörde (K6)** – `A small green microcontroller board plugged into a white USB wall charger on a bookshelf, subtle LED glow, calm atmosphere. 5 seconds.`
1. **Açılış (sahne 1)** – `Close-up of a small green ESP32 microcontroller development board on a wooden desk, slow dolly in, then cut to a warm living room where a table lamp turns on softly. 6 seconds.`
2. **Bağlantı atmosferi (sahne 5)** – `Overhead shot of hands connecting jumper wires between a small microcontroller board and a relay module on a clean workbench, calm and precise movements, soft daylight. 6 seconds.`
3. **Lamba yanması (sahne 8)** – `A bedside lamp on a nightstand turns on with a gentle warm glow, a hand holding a smartphone enters frame from the side but the phone screen is not visible. 5 seconds.`
4. **Kapanış arka planı (sahne 10)** – `Slow pan across a cozy room with several lamps glowing warm yellow at dusk, peaceful atmosphere. 6 seconds.`

İpuçları: her kliği 5–8 saniye tut, gerekirse üç sürüm üretip en iyisini seç. Karttaki yazıları ya da telefonun ekranını istemde tarif etme. Aynı stil satırını her istemde kullan ki sahneler birbirine benzesin.

## Seslendirme
Metinleri bu belgedeki tablodan al. Türkçe seslendirme için Gemini'nin ses üretimi, ElevenLabs ya da benzeri bir araç kullan. Hız hafif yavaş, cümleler kısa kalsın.

## Birleştirme
CapCut, DaVinci Resolve ya da iMovie ile: YZ klipleri + gerçek ekran kayıtları + grafik kartlar + seslendirme + hafif müzik. Gerçek ekran kayıtlarında WiFi adı ve token'ı bulanıklaştır.
