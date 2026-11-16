# Video: Sıfırdan bir ışık düğümü kurmak ve eşleştirmek

**Amaç:** ESP'den hiç anlamayan biri, boş bir ESP32'yi alıp 5 dakikada uygulamaya ışık olarak eklesin.
**Süre:** 3:30 – 4:30 dk. **Dil:** Türkçe, sakin ve kısa cümleler. **Format:** 16:9, 1080p; telefon ekranı dikey kutuda ya da yan yana.
**Yayın yeri:** YouTube (liste dışı da olur), `flash.matestudios.online` sayfasına ve README'ye bağlantı.

> Not: Bu bir **taslak senaryo**. Gerçek çekim, köprü ile düğüm eşleşmesi donanımda doğrulandıktan sonra yapılacak. Ekranlar uygulamanın güncel sürümüne göre kaydedilmeli.

## Gerekenler (video başında ekranda listelenir)
- Bir ESP32 kartı (S3 / C3 / C6 / S2 / klasik) ve veri kablosu
- Röle modülü (tercihen ters mantık, 5V) ve bir lamba
- Nowlight köprüsü kurulu ve evin WiFi'sinde
- Chrome ya da Edge'li bir bilgisayar (veya Android telefon)
- Nowlight uygulaması yüklü iPhone

## Sahneler

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
