# Sorun giderme ve öğrenilen dersler

> 🇬🇧 English version: [en/troubleshooting.md](en/troubleshooting.md)

Geliştirme sırasında yaşadığımız sorunlar ve çözümleri.

## Ekran

| Belirti | Sebep | Çözüm |
|---|---|---|
| Görüntü yenilenmiyor, butonlar tepkisiz görünüyor (dokunma log'da çalışıyor) | LVGL çizim tamponunun **satır sayısı tek** | Tampon yüksekliğini çift yap (`lcd_config.h`): panel çift başlangıç/tek bitiş ister. Hem dikey hem yatay çözünürlükte çift olmalı |
| Ekran döndürünce bozuk/kırmızı görüntü | LVGL `sw_rotate`/`transform_angle` bu sürücüyle uyumsuz | Kendi flush döndürmesini kullan (`lcd_bsp.c`) |
| Dokunma ve kaydırma takılıyor, "parmağı algılamıyor" | Log yazmak (`Serial.printf`) **USB CDC'de terminal bağlı değilken bloklanır**, LVGL görevini durdurur | `Serial.setTxTimeoutMs(0)` çağır ve LVGL görevi içinde gereksiz log yazma |
| Dokunmada arada tek tük kopma | FT3168 arada boş okuma döndürür | En fazla 2 ardışık boş okumayı yok say (kodda var) |

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

## Kumanda (Waveshare)

| Belirti | Sebep | Çözüm |
|---|---|---|
| Otomatik dönme çalışmıyor, ivme değerleri saçma (bir eksen doygun) | Kartın ivmeölçeri arızalı olabilir | Yalnızca jiroskop kullan (bu projenin yaptığı gibi), ya da Z ekseni ile yatay-lock için bak |
| Elde tutarken dönmüyor | Sırt üstü kilit çok hassastı (hafif arkaya yatık tutuş) | Eşik `FLAT_ON_G = 0.97` ve 1 sn şartı ile sıkılaştırıldı |
| Kısa dokunuş uykuyu uyandırmıyor | Derin uykuda 300 ms'de bir bakılıyor | Ekrana ~0,5 sn basılı tut |
| USB takılıyken uyumuyor | Tasarım gereği (harici güç varken uyku kapalı) | Test için `-DSLEEP_TEST_ON_USB=1` |
| Pil bitiyor | ESP-NOW için radyo açık, uyku 90 sn sonra | Kullanmıyorken dock'a koy, uyku zamanını `IDLE_SLEEP_MS` ile kısalt |

## Araçlar

| Belirti | Çözüm |
|---|---|
| CH340'lı kartta `esptool` "Invalid head of packet" / bozuk veri | Yükleme/okuma hızını **115200**'e düşür (`upload_speed = 115200`) |
| ESP32-S3 Super Mini yüklenince "waiting for download" | **BOOT** düğmesi basılıyken takılmış. BOOT'a basmadan yeniden tak |
| `pio device monitor` "could not exclusively lock port" | Başka bir işlem (eski bir monitör penceresi) portu tutuyor. Onu Ctrl+C ile kapat |
| Seri portu açınca kart yeniden başlıyor | Normal (DTR/RTS ile reset). Log'u kaçırmamak için önce portu aç |
