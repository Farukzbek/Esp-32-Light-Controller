# Telefon köprüsü (klasik ESP32 devkit)

iOS uygulaması ve widget'ları ESP-NOW konuşamadığı için, evin WiFi'sine bağlanan bir ESP32 köprü olur: telefondan gelen komutu ESP-NOW ile masa ve yatak düğümlerine iletir, durumu geri okur. **Yalnızca ev ağında çalışır** (internete açılmaz).

## Önemli: kanal
ESP32'nin tek radyosu var. WiFi'ye bağlıyken ESP-NOW da o kanalda çalışır. Bu yüzden router'ın 2.4 GHz kanalını **`NOW_CHANNEL`** değerine (varsayılan 1) **sabitle**. Köprü bağlanınca kanallar farklıysa seri günlükte uyarı verir, `/api/state` de `wifiChannel` ile `channel` değerlerini gösterir.

## Kurulum
1. Köprünün MAC adresini oku, `firmware/shared/now_config.h` içine `#define NOW_BRIDGE_MAC "..."` olarak ekle (örnek satır `now_config.example.h`'de yorumda).
2. **Masa ve yatak düğümlerini yeniden yükle** (ikinci kumandayı tanımaları için). CYD'ye dokunmak gerekmez.
3. Köprüyü yükle: `cd firmware/bridge && pio run -t upload --upload-port <PORT>`
4. Seri monitörde (115200) WiFi bilgisini gir. Şifre kodda ya da depoda tutulmaz, yalnızca cihazın NVS'inde saklanır:
   ```
   wifi <ağ_adı> <şifre>
   token
   ```
   `token` komutu telefon uygulamasına girilecek anahtarı gösterir (ilk açılışta rastgele üretilir, `newtoken` ile yenilenir).

Seri komutlar: `wifi`, `token`, `newtoken`, `status`, `reboot`.

## HTTP arayüzü
Token, `X-Token` başlığında gönderilir (`/api/ping` hariç).

| İstek | Anlam |
|---|---|
| `GET /api/ping` | `{"name":"nowlight-bridge"}` (token gerekmez) |
| `GET /api/state` | `{"masa":0\|1\|-1,"yatak":…,"led":…,"links":{"masa":true,"yatak":true},"channel":1,"wifiChannel":1}` (-1: henüz bildirilmedi) |
| `POST /api/set?dev=masa\|yatak\|led\|all&val=0\|1` | Komut gönderir. Gerçek onay, bir sonraki `/api/state`'te düğümün bildirdiği değerdir. |

Adres: `nowlight.local` (mDNS) ya da cihazın IP'si.
