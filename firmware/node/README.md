# Nowlight genel ışık düğümü

Tüm kartlarda **aynı yazılım**: içinde hiçbir ayar yok. İlk açılışta köprüyle eşleşmeyi bekler, eşleşince bir röle (ışık) olur.

## Yükleme (kolay yol): web yükleyici
**https://flash.matestudios.online** (Chrome ya da Edge). Kartı seç, USB ile tak, Yükle.

Yerelde denemek için:
`web/flasher` klasörünü bir web sunucusuyla aç (Chrome ya da Edge, bilgisayarda/Android'de):
```bash
cd web/flasher && python3 -m http.server 8765      # sonra http://localhost:8765
```
Kartını listeden seç, USB ile tak, **Yükle**'ye bas. (İnternette bir yere koyunca `https` ile aynı şekilde çalışır.)

Yazılımı yeniden derleyip web paketini güncellemek için: `cd firmware/node && ./build_web.sh`

## Yükleme (PlatformIO)
```bash
cd firmware/node
pio run -e esp32s3-supermini -t upload      # ya da esp32dev, esp32c6
```
Kartlar ve röle pini:

| Ortam | Kart | Röle | BOOT |
|---|---|---|---|
| `esp32s3-supermini` | ESP32-S3 Super Mini | GPIO5 | GPIO0 |
| `esp32dev` | Klasik ESP32 DevKit | GPIO23 | GPIO0 |
| `esp32c6` | ESP32-C6 DevKit | GPIO5 | GPIO9 |

Röle mantığı **ters** (LOW = açık). Elektrik gelince ışık kapalı başlar.

## Eşleştirme
1. Köprüde bu sürümün yüklü olması gerekir (`firmware/bridge`).
2. Düğüme güç ver. Seri monitörde (115200) `ESLESME BEKLIYOR` yazar.
3. Telefonda **Cihaz Ekle → Cihaz ekle → Cihaz ara**: düğüm `Nowlight-XXXX` adıyla listelenir. Seç, ad ve simge ver, **Bağla**.
4. Düğüm yeniden başlar ve köprüyle şifreli konuşmaya başlar. **Deneme** adımında ışığı aç/kapat.

Eşleşmeyi silmek: BOOT tuşuna 5 sn basılı tut (ya da açılışta 3 sn), ya da seri monitörde `reset`.

## Güvenlik
Eşleşme penceresi (uygulamadan açılır, 2 dk) dışında ağ parolası havadan geçmez. Pencerede parola, X25519 anahtar değişimi + AES-256-GCM ile şifreli gider. Pencerede araya giren **aktif** bir saldırgan bu yöntemle engellenmez; pencereyi yalnızca kendin açıp kapat. Ayrıntı: `firmware/shared/pair_proto.h`.

## Sınırlar
- Köprü en fazla **4** eşleşen düğümle konuşur (ESP-NOW 6 şifreli eş: hub + yatak + 4). Yani toplam 7 ışık.
- Köprünün WiFi kanalı ile düğüm aynı kanalda buluşur; düğüm kanalı kendi tarar, ayar gerekmez.
- Dokunmatik/sensör girişi yok, yalnızca röle.
