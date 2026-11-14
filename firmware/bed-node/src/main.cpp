#include "HomeSpan.h"
#include "now_proto.h"

// Yatak lambasi: ESP-NOW ile kumandadan komut alir, rolesi (GPIO5, ters mantik: LOW = acik) surer.
// WiFi'ye / router'a / HomeKit'e BAGLANMAZ. Uc cihaz NOW_CHANNEL kanalinda sabit bulusur.
// Elektrik gelince lamba her zaman KAPALI baslar.
// Ek olarak kapasitif dokunma (GPIO9, aluminyum folyo): kumanda olmadan da lamba acilip kapanir.
// Dokunma mantigi masa dugumuyle ayni: acilista 3 sn kalibrasyon, yavas baseline takibi, 3 ardisik okuma, 400 ms bekleme.

#define RELAY_PIN 5
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

#define TOUCH_PIN        9     // ESP32-S3'te GPIO1..14 dokunmatiktir (T1..T14)
#define TOUCH_DELTA_PCT  20    // baseline'a gore % sapma = dokunma. ESP32-S3'te dokununca deger YUKSELIR
#define TOUCH_DEBUG      0     // 1: dokunma degerlerini seri porta yaz (esigi ayarlamak icin)
#define PUSH_QUIET_MS    25000 // kumanda bu kadar sessizse (uykuda/kapali) kendiliginden bildirim atma

static SpanPoint *g_ctrl = nullptr;   // kumanda (komutlar / durum)
static bool g_on = false;
static float g_baseline = 0;
static uint32_t g_lastCtrlRx = 0;

static void relaySet(bool on) {
  g_on = on;
  digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

static void touchCalibrate() {
  Serial.println("\n=== TOUCH KALIBRASYONU: 3 sn dokunma ===");
  delay(3000);
  // ESP32-S3'te sensor ilk okumalarda cok yuksek degerler verir (ortalamayi sisirir): isinma okumalarini at, ortanca al.
  for (int i = 0; i < 30; i++) { touchRead(TOUCH_PIN); delay(10); }
  uint32_t v[40];
  for (int i = 0; i < 40; i++) { v[i] = touchRead(TOUCH_PIN); delay(10); }
  for (int i = 1; i < 40; i++) { const uint32_t x = v[i]; int j = i - 1; while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; } v[j + 1] = x; }
  g_baseline = (v[19] + v[20]) / 2.0f;
  Serial.printf("Baseline: %.0f (en kucuk %lu, en buyuk %lu)\n", g_baseline, (unsigned long)v[0], (unsigned long)v[39]);
}

static void sendState(uint16_t seq) {
  if (seq == 0 && (g_lastCtrlRx == 0 || millis() - g_lastCtrlRx > PUSH_QUIET_MS)) {   // kumanda sessiz: bosuna bekleme
    Serial.println("[now] kumanda sessiz, kendiliginden bildirim atlandi");
    return;
  }
  NowMsg r = {};
  r.type = NOW_STATE;
  r.st[DEV_MASA]  = NOW_UNKNOWN;
  r.st[DEV_YATAK] = g_on ? 1 : 0;
  r.st[DEV_LED]   = NOW_UNKNOWN;
  r.st[3]         = NOW_UNKNOWN;
  r.seq = seq;
  const boolean ok = g_ctrl->send(&r);   // kilitli kanalda kumanda uykudaysa hizla basarisiz olur, sorun degil
  Serial.printf("[now] STATE yatak=%d seq=%u -> %s\n", r.st[DEV_YATAK], seq, ok ? "ok" : "ulasilamadi");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Role acilista gitmesin: cikisi acmadan once seviyeyi HIGH (kapali) yap
  gpio_set_level((gpio_num_t)RELAY_PIN, 1);
  pinMode(RELAY_PIN, OUTPUT);
  relaySet(false);

  touchCalibrate();

  SpanPoint::setPassword(NOW_PASSWORD);
  g_ctrl = new SpanPoint(NOW_CONTROLLER_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);
  homeSpan.setLogLevel(0);
  SpanPoint::setChannelMask(1 << NOW_CHANNEL);   // radyoyu sabit kanala kilitle
  Serial.printf("\nYATAK DUGUMU  MAC = %s  sabit kanal = %d\n", Network.macAddress().c_str(), NOW_CHANNEL);
}

static void touchLoop() {
  static bool touched = false;
  static uint8_t activeCount = 0;
  static uint32_t lastRead = 0, holdOff = 0;

  if (millis() - lastRead < 30) return;
  lastRead = millis();
  const float v = touchRead(TOUCH_PIN);
#if TOUCH_DEBUG
  static uint32_t lastDbg = 0;
  if (millis() - lastDbg > 500) { lastDbg = millis(); Serial.printf("[touch] deger=%.0f baseline=%.0f\n", v, g_baseline); }
#endif
  const float d = g_baseline * TOUCH_DELTA_PCT / 100.0f;
  const bool active  = v > g_baseline + d;          // dokunma
  const bool release = v < g_baseline + d / 2;      // birakma (histerezis)

  if (!touched) {
    if (active) {
      if (++activeCount >= 3 && millis() > holdOff) {   // 3 ardisik okuma: parazit filtresi
        touched = true;
        holdOff = millis() + 400;
        relaySet(!g_on);
        Serial.printf("[touch] lamba -> %s\n", g_on ? "ACIK" : "KAPALI");
        sendState(0);
      }
    } else {
      activeCount = 0;
      g_baseline += 0.002f * (v - g_baseline);          // yavas kayma takibi
    }
  } else if (release) {
    touched = false;
    activeCount = 0;
  }
}

void loop() {
  touchLoop();
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
  delay(5);
}
