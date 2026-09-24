#include "HomeSpan.h"
#include "now_proto.h"

// Masa lambasi dugumu / hub (ESP32-S3 Super Mini): kapasitif dokunma bandi + role + ESP-NOW ile kumanda ile
// iki yonlu haberlesme. WiFi'ye / router'a BAGLANMAZ. Uc cihaz NOW_CHANNEL kanalinda sabit bulusur.
// (Yazarin kendi dugumu klasik ESP32'dir; ayni mantik orada donanimda test edildi. S3 surumu DONANIMDA TEST EDILMEDI.)

// ---- Donanim ayarlari (kendi kablolamana gore degistir) ----
#define RELAY_PIN        5
#define RELAY_ACTIVE_LOW 1     // 1: IN=LOW iken role calisir (cogu 5V role modulu)
#define TOUCH_PIN        4     // ESP32-S3'te GPIO1..14 dokunmatiktir (T1..T14)
#define TOUCH_DELTA_PCT  20    // baseline'a gore % sapma = dokunma (bant boyutuna gore ayarla)
#define TOUCH_DEBUG      0     // 1: dokunma degerlerini seri porta yaz (esigi ayarlamak icin)

// Dokununca okuma degeri ESP32-S3'te YUKSELIR, klasik ESP32'de DUSER.
#if CONFIG_IDF_TARGET_ESP32S3
  #define TOUCH_RISES_ON_TOUCH 1
#else
  #define TOUCH_RISES_ON_TOUCH 0
#endif

#define RELAY_ON  (RELAY_ACTIVE_LOW ? LOW : HIGH)
#define RELAY_OFF (RELAY_ACTIVE_LOW ? HIGH : LOW)
#define PUSH_QUIET_MS 25000   // kumanda bu kadar sessizse (uykuda) kendiliginden bildirim atma

static float g_baseline = 0;
static SpanPoint *g_ctrl = nullptr;
static uint32_t g_lastCtrlRx = 0;
static bool g_on = false;
static uint8_t g_virt_led = 0;   // LED: henuz gercek cihaz yok, sadece durum tutulur

static void relaySet(bool on) {
  g_on = on;
  digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

static void touchCalibrate() {
  Serial.println("\n=== TOUCH KALIBRASYONU: 3 sn dokunma ===");
  delay(3000);
  uint32_t sum = 0;
  for (int i = 0; i < 40; i++) { sum += touchRead(TOUCH_PIN); delay(10); }
  g_baseline = sum / 40.0f;
  Serial.printf("Baseline: %.0f\n", g_baseline);
}

// Kumandaya durumu gonderir (seq: yanit verdigimiz istegin numarasi, kendiliginden ise 0)
static void sendState(uint16_t seq) {
  NowMsg r = {};
  r.type = NOW_STATE;
  r.st[DEV_MASA]  = g_on ? 1 : 0;
  r.st[DEV_YATAK] = NOW_UNKNOWN;         // yatak lambasi kendi dugumunden yonetilir
  r.st[DEV_LED]   = g_virt_led;
  r.st[3]         = NOW_UNKNOWN;
  r.seq = seq;
  if (seq == 0 && (g_lastCtrlRx == 0 || millis() - g_lastCtrlRx > PUSH_QUIET_MS)) {   // kumanda uykuda: bosuna bekleme
    Serial.println("[now] kumanda sessiz, kendiliginden bildirim atlandi");
    return;
  }
  const boolean ok = g_ctrl->send(&r);
  Serial.printf("[now] STATE gonderildi masa=%d led=%d seq=%u -> %s\n", r.st[0], r.st[2], seq, ok ? "ok" : "ulasilamadi");
}

static void handleNow() {
  NowMsg m;
  while (g_ctrl->get(&m)) {
    g_lastCtrlRx = millis();
    if (m.type == NOW_SET && m.dev < DEV_COUNT) {
      Serial.printf("[now] SET dev=%d val=%d seq=%u\n", m.dev, m.val, m.seq);
      if (m.dev == DEV_MASA) relaySet(m.val ? 1 : 0);
      else if (m.dev == DEV_LED) g_virt_led = m.val ? 1 : 0;
      sendState(m.seq);
    } else if (m.type == NOW_QUERY) {
      sendState(m.seq);
    }
  }
}

static void touchLoop() {
  static bool touched = false;
  static uint8_t lowCount = 0;
  static uint32_t lastRead = 0, holdOff = 0;

  if (millis() - lastRead < 30) return;
  lastRead = millis();
  const float v = touchRead(TOUCH_PIN);
#if TOUCH_DEBUG
  static uint32_t lastDbg = 0;
  if (millis() - lastDbg > 500) { lastDbg = millis(); Serial.printf("[touch] deger=%.0f baseline=%.0f\n", v, g_baseline); }
#endif
  const float d = g_baseline * TOUCH_DELTA_PCT / 100.0f;
  const bool low  = TOUCH_RISES_ON_TOUCH ? (v > g_baseline + d) : (v < g_baseline - d);          // dokunma
  const bool high = TOUCH_RISES_ON_TOUCH ? (v < g_baseline + d / 2) : (v > g_baseline - d / 2);  // birakma (histerezis)

  if (!touched) {
    if (low) {
      if (++lowCount >= 3 && millis() > holdOff) {
        touched = true;
        holdOff = millis() + 400;
        relaySet(!g_on);
        Serial.printf("[touch] lamba -> %s\n", g_on ? "ACIK" : "KAPALI");
        sendState(0);
      }
    } else {
      lowCount = 0;
      g_baseline += 0.002f * (v - g_baseline);   // yavas kayma takibi
    }
  } else if (high) {
    touched = false;
    lowCount = 0;
  }
}

void setup() {
  Serial.begin(115200);

  // Role acilista gitmesin: cikisi acmadan once seviyeyi HIGH (kapali) yap
  gpio_set_level((gpio_num_t)RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT);
  relaySet(false);

  touchCalibrate();

  SpanPoint::setPassword(NOW_PASSWORD);
  g_ctrl = new SpanPoint(NOW_CONTROLLER_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);
  homeSpan.setLogLevel(0);
  SpanPoint::setChannelMask(1 << NOW_CHANNEL);   // radyoyu sabit kanala kilitle
  Serial.printf("MASA DUGUMU  MAC = %s  sabit kanal = %d\n", Network.macAddress().c_str(), NOW_CHANNEL);
}

void loop() {
  handleNow();
  touchLoop();
  delay(5);
}
