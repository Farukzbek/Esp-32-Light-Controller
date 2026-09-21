#include "HomeSpan.h"
#include "now_proto.h"
#include "esp_wifi.h"

// Yatak lambasi: ESP-NOW ile kontrolcuden komut alir, rolesi (GPIO5, ters mantik: LOW = acik) surer.
// Bu cihaz HomeKit'e / WiFi'ye baglanmaz. Elektrik gelince lamba her zaman KAPALI baslar.
//
// Kanal: radyo tek kanala KILITLI tutulur (SpanPoint::send() yanit gelmezse tum kanallari tarar ve
// NVS'ye yazar; kontrolcu uykudayken bu surekli olur). Hub (masa ESP32) hep uyanik ve router kanalinda
// oldugu icin kanal capasi olarak onu kullaniriz: hub yanit vermezse tek seferlik tarama yapip onun kanalina geceriz.

#define RELAY_PIN 5
#define RELAY_ON  LOW
#define RELAY_OFF HIGH
#define DEFAULT_CHANNEL 10          // ilk acilis icin tahmin (router kanali); yanlissa hub taramasi duzeltir
#define ANCHOR_PERIOD_MS 20000

static SpanPoint *g_ctrl = nullptr;   // kontrolcu (komutlar / durum)
static SpanPoint *g_hub  = nullptr;   // hub: sadece kanal capasi (hub'dan mesaj beklemiyoruz)
static uint8_t g_ch = DEFAULT_CHANNEL;
static bool g_on = false;
static uint32_t g_lastCtrlRx = 0;   // kontrolcuden son mesaj (konusma sirasinda kanal taramasi yapma: tarama ~1 sn sagir birakir)

static void relaySet(bool on) {
  g_on = on;
  digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

static void lockChannel(uint8_t ch) {
  if (ch < 1 || ch > 13) ch = g_ch;
  SpanPoint::setChannelMask(1 << ch);   // radyoyu da bu kanala alir, tarama yok
  g_ch = ch;
}

static NowMsg makeState(uint16_t seq) {
  NowMsg r = {};
  r.type = NOW_STATE;
  r.st[DEV_MASA]  = NOW_UNKNOWN;
  r.st[DEV_YATAK] = g_on ? 1 : 0;
  r.st[DEV_LED]   = NOW_UNKNOWN;
  r.st[3]         = NOW_UNKNOWN;
  r.seq = seq;
  return r;
}

// Kontrolcuye durumumuzu bildir (kilitli kanalda hizli basarisiz olur, kontrolcu uykudaysa sorun degil)
static void sendState(uint16_t seq) {
  NowMsg r = makeState(seq);
  boolean ok = g_ctrl->send(&r);
  Serial.printf("[now] STATE yatak=%d seq=%u -> %s\n", r.st[DEV_YATAK], seq, ok ? "ok" : "ulasilamadi");
}

// Modemin kanalini WiFi taramasiyla oku (hub bu kanalda, kontrolcu de). Mesaj onayina bagli degil, NVS'ye yazmaz.
static uint8_t scanRouterChannel() {
  wifi_scan_config_t cfg = {};
  cfg.ssid = (uint8_t *)NOW_ROUTER_SSID;
  cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  cfg.scan_time.active.min = 40;
  cfg.scan_time.active.max = 80;
  uint8_t best = 0;
  if (esp_wifi_scan_start(&cfg, true) == ESP_OK) {          // bloklar (~1.5 sn)
    wifi_ap_record_t recs[8];
    uint16_t m = 8;
    if (esp_wifi_scan_get_ap_records(&m, recs) == ESP_OK) {
      int bestRssi = -127;
      for (int i = 0; i < m; i++) {
        if (strcmp((const char *)recs[i].ssid, NOW_ROUTER_SSID) == 0 && recs[i].primary >= 1 && recs[i].primary <= 13 && recs[i].rssi > bestRssi) {
          best = recs[i].primary; bestRssi = recs[i].rssi;
        }
      }
    }
  }
  return best;
}

static void syncChannel() {
  const uint32_t t0 = millis();
  const uint8_t ch = scanRouterChannel();
  Serial.printf("[now] kanal taramasi: %d (%lu ms)\n", ch, (unsigned long)(millis() - t0));
  if (ch == 0) { Serial.println("[now] modem SSID'si bulunamadi, mevcut kanalda kaliniyor"); lockChannel(g_ch); return; }
  if (ch != g_ch) Serial.printf("[now] modem kanali %d (eski %d): kilitleniyor\n", ch, g_ch);
  lockChannel(ch);
}

// Hub'a ulasabiliyor muyuz? (kilitli kanalda, sadece radyo onayi). 2 ardisik basarisizlikta modem kanalini yeniden tara.
static void anchorTick() {
  static int fails = 0;
  NowMsg m = makeState(0);
  if (g_hub->send(&m)) { fails = 0; return; }
  if (++fails >= 3 && millis() - g_lastCtrlRx > 8000) { fails = 0; syncChannel(); }   // kontrolcuyle konusuyorken tarama yapma
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Role acilista gitmesin: cikisi acmadan once seviyeyi HIGH (kapali) yap
  gpio_set_level((gpio_num_t)RELAY_PIN, 1);
  pinMode(RELAY_PIN, OUTPUT);
  relaySet(false);

  Serial.printf("\nYATAK ESP  MAC = %s\n", Network.macAddress().c_str());
  SpanPoint::setPassword(NOW_PASSWORD);
  g_ctrl = new SpanPoint(NOW_CONTROLLER_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);
  g_hub  = new SpanPoint(NOW_HUB_MAC, sizeof(NowMsg), 0);
  homeSpan.setLogLevel(0);
  lockChannel(g_ch);
  syncChannel();                     // acilista modemin kanalini bul
}

void loop() {
  static uint32_t nextAnchor = 0, nextSync = 300000;

  NowMsg m;
  while (g_ctrl->get(&m)) {
    g_lastCtrlRx = millis();
    if (m.type == NOW_SET && m.dev == DEV_YATAK) {
      relaySet(m.val ? 1 : 0);
      Serial.printf("[now] SET yatak=%d seq=%u\n", m.val, m.seq);
      sendState(m.seq);
    } else if (m.type == NOW_QUERY) {
      sendState(m.seq);
    }
  }

  if (millis() >= nextAnchor) {
    nextAnchor = millis() + ANCHOR_PERIOD_MS;
    anchorTick();
  }
  if (millis() >= nextSync) {          // 5 dakikada bir modem kanalini dogrula (kanal degismis olabilir)
    if (millis() - g_lastCtrlRx > 8000) { nextSync = millis() + 300000; syncChannel(); }
    else nextSync = millis() + 10000;   // konusuyoruz: biraz sonra tekrar dene
  }
  delay(5);
}
