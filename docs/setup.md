# Kurulum

## Gereksinimler

- [PlatformIO](https://platformio.org/) (`pio` komutu, CLI yeterli)
- USB veri kablosu, ilk yüklemeler için
- Apple Home için bir iPhone/iPad, hub ile aynı WiFi ağında

Yazılımlar [pioarduino](https://github.com/pioarduino/platform-espressif32) platformunu kullanır (Arduino-ESP32 3.x). Resmi `espressif32` platformu QSPI ekran API'leri için fazla eski kalır. Kütüphaneler (`LVGL 8.3.11`, `HomeSpan`) PlatformIO tarafından otomatik indirilir.

## 1) Kişisel ayarlar

```bash
cp firmware/shared/now_config.example.h firmware/shared/now_config.h
```

`now_config.h` `.gitignore`'dadır, depoya gitmez. İçini doldur:

| Ayar | Ne |
|---|---|
| `NOW_HUB_MAC`, `NOW_CONTROLLER_MAC`, `NOW_BED_MAC` | Üç cihazın Wi-Fi STA MAC'i |
| `NOW_ROUTER_SSID` | **Hub'ın bağlanacağı** 2,4 GHz WiFi ağının adı |
| `NOW_PASSWORD` | ESP-NOW şifreleme parolası, üç cihazda aynı. Değiştir |

**MAC adreslerini** kart USB'ye takılıyken okuyabilirsin:

```bash
pio pkg exec -p tool-esptoolpy -- esptool --port <PORT> read-mac
```

(`<PORT>`: macOS'ta `/dev/cu.usbmodemXXXX`, Linux'ta `/dev/ttyACM0`.) Ya da MAC'leri bilmeden önce üç cihaza da bir kez yükle: **açılış seri log'unda MAC yazar** (kumanda `[now] kumanda MAC = ...`, yatak düğümü `YATAK ESP  MAC = ...`, hub HomeSpan başlık bilgisinde `MAC Address: ...`). Sonra `now_config.h`'a yazıp yeniden yükle.

## 2) Yükleme

Her klasörde:

```bash
cd firmware/controller   # ya da hub, bed-node
pio run -t upload --upload-port <PORT>
```

- **Kumanda** (Waveshare): USB-C ile bağla.
- **ESP32-S3 Super Mini** (hub ve yatak): USB-C ile bağla. Port görünmezse **BOOT** düğmesine basılı tutarak takıp bırak. Yükleme bittikten sonra kartı **BOOT'a basmadan** yeniden tak (BOOT basılıyken kart indirme modunda kalır, program çalışmaz).

## 3) Hub'ın WiFi'sini gir ve Apple Home'a ekle

Hub ilk açılışta WiFi bilgisini bilmez. HomeSpan'ın seri komut satırıyla gir (şifre cihazın hafızasında kalır, kodda yer almaz):

```bash
pio device monitor -b 115200 -p <PORT>
```

1. Terminale tıkla, **bir kez Enter**'a bas.
2. **Büyük `W`** yaz (başında/sonunda boşluk ya da ok tuşu olmasın), Enter.
3. Çevredeki ağlar numaralı listelenir, **numarayı** (ya da ağ adını) yaz, Enter.
4. Şifreyi yaz, Enter. Cihaz kaydedip yeniden başlar ve `WiFi Connected!` yazar.

Hub açılırken **3 saniye boyunca dokunma bandına dokunma** (kalibrasyon).

Apple Home'da **Aksesuar Ekle**'ye bas, HomeSpan'ın varsayılan kurulum koduyla (`466-37-726`) ekle. HomeSpan CLI'da `?` yazarsan komutları görürsün. Kendi kodunu belirlemek için `homeSpan.setPairingCode(...)` kullan.

## 4) Router ayarı (önemli)

ESP-NOW kanalı, hub'ın router'a bağlandığı kanaldır. Router'ın 2,4 GHz kanalını **"Otomatik"ten sabit bir kanala (1, 6 ya da 11)** al, kanal genişliğini 20 MHz yap. Otomatik kanal seçen router, yeniden başlayınca kanal değiştirip ESP-NOW bağlantısını bir süre koparabilir. Kumanda ve yatak düğümü kanalı `NOW_ROUTER_SSID`'yi tarayıp kendiliğinden bulur, yine de kanalın sabit olması sorunları azaltır. Hub'a modemden sabit IP (DHCP rezervasyonu) vermek de faydalıdır.

Hub'ı zayıf sinyalli bir yere koyacaksan (`Durum` sayfasında hub `-80 dBm`'den kötü görünüyorsa) WiFi'li bir powerline adaptörü ya da repeater kullan. Bunu yaparsan `NOW_ROUTER_SSID` **hub'ın gerçekte bağlandığı** ağın adı olmalı ve o cihazın kanalı router'la aynı sabit kanala ayarlanmalıdır.

## 5) İlk açılış kontrol listesi

- [ ] Hub seri log'unda `[wifi] baglandi: '...' kanal N` görünüyor.
- [ ] Kumanda log'unda `[now] kanal kilitlendi: N` (hub ile aynı kanal).
- [ ] Kumandadan **Masa** düğmesi röleyi tıklatıyor, ekran yeşil/kırmızı doğru gösteriyor.
- [ ] Dokunma bandı lambayı açıp kapatıyor, kumanda ekranı güncelleniyor.
- [ ] Apple Home'dan aç/kapat kumandaya yansıyor.
- [ ] Yatak düğümünü yükledin, rölesi kumandadan tık yapıyor (**220V bağlamadan önce**).
- [ ] Kumanda **Durum** sayfasında hub/yatak dBm görünüyor.

Sorun çıkarsa [troubleshooting.md](troubleshooting.md).
