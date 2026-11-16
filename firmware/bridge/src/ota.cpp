#include "ota.h"
#include <WiFi.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_crt_bundle.h>

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

#define OTA_BASE "https://flash.matestudios.online/firmware/"

enum OtaState { OTA_IDLE, OTA_CHECKING, OTA_DOWNLOADING, OTA_DONE, OTA_ERROR };
static volatile OtaState s_state = OTA_IDLE;
static volatile int s_progress = 0;
static String s_latest, s_error;
static volatile bool s_busy = false;

bool ota_supported() {
#ifdef NL_BRIDGE_KEY
  return true;
#else
  return false;
#endif
}

static bool newer(const String &a, const String &b) {                 // a, b'den yeni mi? ("0.5.1" > "0.5.0")
  int pa[3] = {0, 0, 0}, pb[3] = {0, 0, 0};
  sscanf(a.c_str(), "%d.%d.%d", &pa[0], &pa[1], &pa[2]);
  sscanf(b.c_str(), "%d.%d.%d", &pb[0], &pb[1], &pb[2]);
  for (int i = 0; i < 3; i++) if (pa[i] != pb[i]) return pa[i] > pb[i];
  return false;
}

String ota_version_json() {
#ifdef NL_BRIDGE_KEY
  const char *key = NL_BRIDGE_KEY;
#else
  const char *key = "";
#endif
  return String("{\"version\":\"") + FW_VERSION + "\",\"chip\":\"" + key + "\",\"ota\":" + (ota_supported() ? "true" : "false") +
#ifdef NOW_GENERIC
         ",\"generic\":true}";
#else
         ",\"generic\":false}";
#endif
}

String ota_status_json() {
  const char *st[] = {"idle", "checking", "downloading", "done", "error"};
  String e = s_error; e.replace("\"", "'");
  const bool avail = s_latest.length() && newer(s_latest, String(FW_VERSION));
  return String("{\"state\":\"") + st[s_state] + "\",\"current\":\"" + FW_VERSION + "\",\"latest\":\"" + s_latest +
         "\",\"available\":" + (avail ? "true" : "false") + ",\"progress\":" + String((int)s_progress) + ",\"error\":\"" + e + "\"}";
}

#ifdef NL_BRIDGE_KEY
static bool httpsGet(const String &url, String &out) {
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.crt_bundle_attach = esp_crt_bundle_attach;      // Mozilla kok sertifika paketi: sunucu kimligi dogrulanir
  cfg.timeout_ms = 15000;
  esp_http_client_handle_t h = esp_http_client_init(&cfg);
  if (!h) return false;
  bool ok = false;
  if (esp_http_client_open(h, 0) == ESP_OK) {
    esp_http_client_fetch_headers(h);
    const int status = esp_http_client_get_status_code(h);
    char buf[256];
    int n;
    out = "";
    while ((n = esp_http_client_read(h, buf, sizeof(buf) - 1)) > 0 && out.length() < 4000) { buf[n] = 0; out += buf; }
    ok = status == 200;
  }
  esp_http_client_cleanup(h);
  return ok;
}

static void checkTask(void *) {
  const String url = String(OTA_BASE) + "bridge-" + NL_BRIDGE_KEY + ".json?t=" + String(millis());
  String body;
  if (!httpsGet(url, body)) { s_error = "Güncelleme sunucusuna ulaşılamadı"; s_state = OTA_ERROR; s_busy = false; vTaskDelete(nullptr); return; }
  const int i = body.indexOf("\"version\"");
  const int q1 = body.indexOf('"', body.indexOf(':', i) + 1), q2 = body.indexOf('"', q1 + 1);
  if (i < 0 || q1 < 0 || q2 < 0) { s_error = "Sürüm bilgisi okunamadı"; s_state = OTA_ERROR; }
  else { s_latest = body.substring(q1 + 1, q2); s_error = ""; s_state = OTA_IDLE; }
  s_busy = false;
  vTaskDelete(nullptr);
}

static void updateTask(void *) {
  const String url = String(OTA_BASE) + "bridge-" + NL_BRIDGE_KEY + "/firmware.bin?v=" + s_latest;
  esp_http_client_config_t cfg = {};
  cfg.url = url.c_str();
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.timeout_ms = 20000;
  esp_https_ota_config_t ota = {};
  ota.http_config = &cfg;
  esp_https_ota_handle_t h = nullptr;
  s_progress = 0; s_error = ""; s_state = OTA_DOWNLOADING;
  esp_err_t e = esp_https_ota_begin(&ota, &h);
  if (e == ESP_OK) {
    while ((e = esp_https_ota_perform(h)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
      const int total = esp_https_ota_get_image_size(h), got = esp_https_ota_get_image_len_read(h);
      s_progress = total > 0 ? (int)((int64_t)got * 100 / total) : 0;
    }
    if (e == ESP_OK && esp_https_ota_is_complete_data_received(h)) e = esp_https_ota_finish(h);
    else { esp_https_ota_abort(h); if (e == ESP_OK) e = ESP_FAIL; }
  }
  if (e == ESP_OK) {
    s_progress = 100; s_state = OTA_DONE;
    Serial.println("[ota] guncelleme yazildi, yeniden baslatiliyor");
    delay(1500);
    ESP.restart();
  }
  s_error = String("Güncelleme başarısız (") + esp_err_to_name(e) + "). Köprü eski sürümde çalışmaya devam ediyor.";
  s_state = OTA_ERROR; s_busy = false;
  Serial.printf("[ota] hata: %s\n", esp_err_to_name(e));
  vTaskDelete(nullptr);
}
#endif

void ota_check() {
#ifdef NL_BRIDGE_KEY
  if (s_busy) return;
  s_busy = true; s_state = OTA_CHECKING; s_error = "";
  xTaskCreate(checkTask, "otachk", 12288, nullptr, 1, nullptr);
#endif
}

bool ota_start() {
#ifdef NL_BRIDGE_KEY
  if (s_busy || !s_latest.length() || !newer(s_latest, String(FW_VERSION))) return false;
  s_busy = true;
  xTaskCreate(updateTask, "otaupd", 16384, nullptr, 1, nullptr);
  return true;
#else
  return false;
#endif
}
