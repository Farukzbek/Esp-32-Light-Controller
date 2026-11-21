# Sorun giderme ve öğrenilen dersler

> 🇬🇧 English version: [en/troubleshooting.md](en/troubleshooting.md)

Geliştirme sırasında yaşadığımız sorunlar ve çözümleri.

## ESP-NOW

| Belirti | Sebep | Çözüm |
|---|---|---|
| Komut gitti gibi ama karşı taraf çalışmadı | `SpanPoint::send() == true` sadece radyo ACK'i demektir | Uygulama seviyesinde onay kullan: aynı `seq` ile dönen `STATE` (bkz. protocol.md) |
| Uykudaki/kapalı bir cihaza gönderince kumanda diğer cihazlarla da konuşamıyor, log sessiz | `SpanPoint` yanıtsız cihaz için **tüm kanalları tarar**, radyo hub'ın kanalından kopar (ve her kanalda NVS'ye yazar) | Radyoyu `setChannelMask(1 << kanal)` ile kilitle. Sabit `NOW_CHANNEL` kullan (bkz. architecture.md) |
| Kumanda/düğümler birbirini görmüyor | `NOW_CHANNEL`, `NOW_PASSWORD` ya da MAC üç cihazda farklı | Üç cihazın `now_config.h` değerlerini karşılaştır, hepsini aynı ayarla yeniden yükle. Her cihazın log'unda "sabit kanal = N" aynı olmalı |
| Bir ağ/router çevresinde sık paket kaybı | Aynı 2,4 GHz kanalında yoğun trafik | `NOW_CHANNEL`'ı boş bir kanala (1, 6, 11) değiştirip üç cihazı yeniden yükle |
| Aynı komut yatakta 2–5 kez işleniyor | ESP-NOW yeniden denemeleri, ACK kaybı | Sorun değil: `SET` mutlak değer taşır (idempotent) |
| ~%10–15 paket/ACK kaybı, ara sıra 1 sn gecikme | Radyo ortamı (metal kutu, röle/220V kabloları yakınında anten, ucuz adaptör) | Düğümü metal ve kablolardan uzağa, anteni açıkta tut, sağlam adaptör kullan. Kumandanın **Durum** sayfasındaki dBm'e bak (−65 üstü iyi, −78 altı sınırda) |
| Yatak lambası ters (ekran "açık" derken sönük) | Lamba rölenin **NC** ucuna bağlanmış | Lambayı **NO**'ya bağla (bkz. hardware.md). Yazılımda tersine çevirme: elektrik kesilince lamba yanar |

## Masa düğümü (hub)

| Belirti | Sebep | Çözüm |
|---|---|---|
| Dokunma bandı hiç/çok hassas çalışıyor | Eşik, kablo/bant boyutuna göre farklı; **S3'te değer yükselir**, klasik ESP32'de düşer | `TOUCH_DEBUG=1` ile değerlere bak, `TOUCH_DELTA_PCT`'yi ayarla |
| Açılışta dokunulunca yanlış durum | Kalibrasyon dokunuşlu ölçüldü | Yeniden başlat, ilk 3 sn elini çek |

## Kumanda paneli (CYD)

| Belirti | Sebep | Çözüm |
|---|---|---|
| Renkler ters / ekran bozuk | Panel varyantı farklı | `platformio.ini`'de `ILI9341_2_DRIVER` + `TFT_INVERSION_ON` ayarını kontrol et (README'deki CYD notları) |
| Dokunma yanlış yeri algılıyor | Kalibrasyon yok/bozuk | Ayarlar → DURUM → "Dokunmatiği kalibre et" |
| Yükleme hata veriyor / yarıda kesiliyor | CH340 yüksek hızda bozuluyor | `upload_speed = 115200` (ayarlı); kabloyu/portu değiştir, gerekirse BOOT'a basılı tutarak yükle |
| Otomatik parlaklık ters çalışıyor | LDR'nin yönü kartta tersine | Yazılım telafi eder; yine de ters ise `hal_light_percent()` içindeki dönüşümü kontrol et |
| Panel "ulaşılamıyor" uyarısı veriyor | `NOW_CONTROLLER_MAC` CYD'nin MAC'i değil / kanal farklı | CYD'nin MAC'ini (`esptool read-mac`) hub ve yatak düğümünün `now_config.h`'ine yaz, `NOW_CHANNEL` aynı olsun, düğümleri yeniden yükle |
| Ekran kapanıyor | Ayarlardaki "Ekranı kapat" süresi | Ayarlar → EKRAN → "Ekranı kapat" → Hiç |

## Araçlar

| Belirti | Çözüm |
|---|---|
| CH340'lı kartta `esptool` "Invalid head of packet" / bozuk veri | Yükleme/okuma hızını **115200**'e düşür (`upload_speed = 115200`) |
| ESP32-S3 Super Mini yüklenince "waiting for download" | **BOOT** düğmesi basılıyken takılmış. BOOT'a basmadan yeniden tak |
| `pio device monitor` "could not exclusively lock port" | Başka bir işlem (eski bir monitör penceresi) portu tutuyor. Onu Ctrl+C ile kapat |
| Seri portu açınca kart yeniden başlıyor | Normal (DTR/RTS ile reset). Log'u kaçırmamak için önce portu aç |
