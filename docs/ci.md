# CI ve web yükleyici yayını

İki GitHub Actions iş akışı var (`.github/workflows`):

- **`ci.yml`:** her push ve pull request'te 5 kart ortamının düğüm yazılımını, köprü yazılımını (örnek `now_config.h` ile) derler ve X25519 test vektörlerini çalıştırır. Ayrıca düğüm kablosuz güncellemesi için hazırlık testleri çalışır (kayıplı sahte hatta aktarım mantığı `firmware/shared/ota_xfer.h` ve `firmware/tools` araçları) ve her düğüm imajının OTA yuvasına sığıp sığmadığı denetlenir (şimdilik uyarı: C3 ve C6 sığmıyor). Bkz. [plan-dugum-ota.md](plan-dugum-ota.md).
- **`flasher.yml`:** `firmware/node`, `firmware/shared` ya da `web/flasher/index.html` değişince tüm kartların `.bin` dosyalarını üretir ve **flash.matestudios.online** (Cloudflare Pages, proje `nowlight-flasher`) adresine yayınlar. Köprü ve kapı düğümü yazılımları (ESP-NOW parolası içerir) yayına girmez.

## Gerekli depo gizlileri (Settings → Secrets and variables → Actions)
| Ad | Değer |
|---|---|
| `CLOUDFLARE_API_TOKEN` | Cloudflare → My Profile → API Tokens → *Create Token* → şablon **Edit Cloudflare Workers** yerine özel: **Account → Cloudflare Pages → Edit** |
| `CLOUDFLARE_ACCOUNT_ID` | Cloudflare paneli sağ kenar çubuğu → Account ID |

Gizli değerler olmadan `ci.yml` çalışır, `flasher.yml` yayın adımında hata verir (derleme adımı yine geçer).

Elle yayınlamak için: `cd firmware/node && ./build_web.sh && ../deploy_flasher.sh`.
