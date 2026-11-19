# Plan: zamanlı otomasyon (köprüde çalışır)

Durum: **1. aşamanın çekirdek mantığı hazır ve testli** (`firmware/shared/schedule.h`, `firmware/tools/test_schedule.c`). Köprü ve uygulama bağlantısı, kararlar netleşince.

## 1. Hedef

"Her sabah 07:30'da perdeyi aç, Pazar hariç", "23:00'te tüm ışıkları kapat" gibi **saate ve güne bağlı** kurallar. Telefon kapalıyken de çalışır, çünkü köprüde işler. Servo için "açıya git" eylemi de var.

Bugünkü kural motoru (kapı düğümü) yalnızca "kapıda hareket olursa" tetiklenir. Zamanlı kurallar buna dokunmaz, yanına eklenir.

## 2. Mimari

```
Uygulama (kuralların asıl kaynağı) --HTTP--> Köprü: kuralları NVS'te saklar, her dakika saate bakar --ESP-NOW--> cihazlar
```

- **Saat:** Köprü NTP ile saati alır (WiFi'ye zaten bağlı). Saat dilimi telefondan gelen **UTC farkı** (dakika); Türkiye'de sabit +180. Saat alınamadıysa kurallar çalışmaz, saat gelince devam eder.
- **Kural metni:** `en:eylem:cihazlar:dakika:gunler:aci;...` (en çok 16 kural). Eylem: 0 çevir, 1 aç, 2 kapat, 3 servo açısı. Cihazlar: `1 << yuva`. Günler: bit0 Pzt ... bit6 Paz.
- **Tetikleme:** her kural, hedef dakikasında **bir kez** çalışır (aynı dakikada ikinci kez çalışmaz, ertesi gün yine çalışır). Köprü yeniden başlarsa o dakikayı aynı dakikada tekrar çalıştırabilir; aç/kapat için zararsızdır.
- **Kaçırılan tetikler:** köprü kapalıyken (elektrik kesintisi) geçen saatler **atlanır** (varsayılan). İstenirse "kısa kesintiden sonra N dakika içinde yakala" ayarı vardır (`catchUpMin`).
- **API:** `GET/POST /api/schedules` (kural metni), `GET /api/time` (köprünün saati ve fark, tanılama için).
- **Uygulama:** "Programlar" ekranı: kural listesi, saat seçici, gün seçici (şimdiki `DayPicker`), cihaz seçici, eylem (aç/kapat/çevir/servo açısı). Telefon kaynak, değişiklikte köprüye gönderilir (kapı kurallarıyla aynı model).

## 3. Aşamalar

| Aşama | İçerik | Test |
|---|---|---|
| **1** | Zamanlı kurallar: köprüde NTP + motor + API, uygulamada ekran | **Çekirdek hazır (bilgisayarda test edildi).** Köprü ve uygulama bağlantısı sonra; donanımda: kural kur, saatinde tetiklensin |
| 2 | Uyku zamanlayıcısı: "30 dk sonra kapat" (tek seferlik, köprüde) | Zamanlayıcı kur, dakika say |
| 3 | Gün doğumu / batımı tetikleyicisi (± dakika) | Telefon konumu (bir kere) gönderir, köprü hesaplar (NOAA formülü, bilgisayarda test edilebilir) |
| 4 | PIR düğümü ve kapı düğümü olaylarını köprü motoruna bağlamak; kapı kurallarını köprüye taşımak | PIR rol düğümü kural çalıştırsın |

## 4. Riskler ve dikkat

- **Saat kayması / NTP yok:** saat alınamazsa kural çalışmaz; uygulamada "köprünün saati alınamadı" uyarısı gösterilir.
- **Kesintide çakışma:** köprü ve kapı düğümü aynı lambaya farklı komut verebilir (zamanlı "kapat" ile kapıdaki "aç"). Son komut kazanır; sorun görülürse kuralların öncelik/çakışma uyarısı eklenir.
- **Servo:** zamanlı kural servoyu bırakılmışken hareket ettirir. Elle oynatılmış perdenin gerçek konumu bilinmediği için kural "açıya git" der, "çevir" servo için anlamlı olmaz (arayüzde servo için çevir sunulmaz).
- **Yarım kalan eşleşme / silinen cihaz:** silinen cihazın biti kurallardan temizlenir (kapı kurallarındaki gibi).

## 5. Kararlar (kullanıcıdan bekleniyor)

1. **Kural kaynağı telefon mu olsun?** Önerim evet (kapı kurallarıyla aynı). Köprü yalnızca saklar ve çalıştırır.
2. **Kaçırılan tetikler:** elektrik kesintisinde atlansın mı (önerim), yoksa 10 dakikaya kadar yakalansın mı?
3. **Saat dilimi:** Türkiye sabit UTC+3 yeterli mi (yaz saati yok)? Önerim evet; telefon farkı gönderdiği için başka ülkede de çalışır.
4. **Kapı düğümü kuralları ileride köprüye taşınsın mı** (4. aşama)? Önerim evet; kapı düğümü yalnızca "hareket oldu" bilgisini gönderir.
