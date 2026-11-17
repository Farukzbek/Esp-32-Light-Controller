#include "HomeSpan.h"
#include "now_proto.h"
#include <Preferences.h>

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

// Role pini ve mantigi derleme degeridir (RELAY_PIN, RELAY_ACTIVE_LOW) ama telefon uygulamasindan degistirilebilir (NOW_CFG, kopruden gelir)
// ve flash'ta (NVS "nlhub") saklanir. Degistirilmediyse derleme degeri gecerlidir.
static uint8_t g_pin = RELAY_PIN;
static bool g_inv = RELAY_ACTIVE_LOW;
static Preferences prefs;
#define RELAY_ON  (g_inv ? LOW : HIGH)
#define RELAY_OFF (g_inv ? HIGH : LOW)
#define PUSH_QUIET_MS 25000   // kumanda bu kadar sessizse (uykuda) kendiliginden bildirim atma

static float g_baseline = 0;
static SpanPoint *g_ctrl = nullptr;
// Kumandalar: [0] panel (CYD), [1] telefon koprusu (NOW_BRIDGE_MAC tanimliysa). Her biriyle ayri ESP-NOW baglantisi.
struct Peer { SpanPoint *sp; uint32_t lastRx; };
static Peer g_peers[2] = {{nullptr, 0}, {nullptr, 0}};
static int g_npeers = 1;
static bool g_on = false;
static uint8_t g_virt_led = 0;   // LED: henuz gercek cihaz yok, sadece durum tutulur

static void relaySet(bool on) {
  g_on = on;
  digitalWrite(g_pin, on ? RELAY_ON : RELAY_OFF);
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
static void sendStateTo(int pi, uint16_t seq) {
  NowMsg r = {};
  r.type = NOW_STATE;
  r.st[DEV_MASA]  = g_on ? 1 : 0;
  r.st[DEV_YATAK] = NOW_UNKNOWN;         // yatak lambasi kendi dugumunden yonetilir
  r.st[DEV_LED]   = g_virt_led;
  r.st[3]         = NOW_UNKNOWN;
  r.seq = seq;
  if (seq == 0 && (g_peers[pi].lastRx == 0 || millis() - g_peers[pi].lastRx > PUSH_QUIET_MS)) {   // kumanda uykuda: bosuna bekleme
    Serial.println("[now] kumanda sessiz, kendiliginden bildirim atlandi");
    return;
  }
  const boolean ok = g_peers[pi].sp->send(&r);
  Serial.printf("[now] STATE gonderildi masa=%d led=%d seq=%u -> %s\n", r.st[0], r.st[2], seq, ok ? "ok" : "ulasilamadi");
}

// seq != 0: istegi yapan kumandaya yanit (sendStateTo); seq == 0: kendiliginden bildirim, tum kumandalara.
static void sendState(uint16_t seq) {
  for (int i = 0; i < g_npeers; i++) sendStateTo(i, seq);
}

// Uygulamadan gelen role ayari: gecerli bir cikis pini olmali, dokunma pinine esit olamaz. Role kapali kalir, eski pin serbest birakilir.
static void applyRelayCfg(uint8_t pin, bool inv) {
  if (!GPIO_IS_VALID_OUTPUT_GPIO(pin) || pin == TOUCH_PIN) { Serial.printf("[now] CFG reddedildi: gecersiz pin %u\n", pin); return; }
  const bool wasOn = g_on;
  if (pin != g_pin) { relaySet(false); pinMode(g_pin, INPUT); g_pin = pin; digitalWrite(g_pin, inv ? HIGH : LOW); pinMode(g_pin, OUTPUT); }
  g_inv = inv;
  relaySet(wasOn && pin == g_pin ? wasOn : false);
  prefs.begin("nlhub", false); prefs.putUChar("pin", g_pin); prefs.putUChar("inv", g_inv ? 1 : 0); prefs.end();
  Serial.printf("[now] CFG role pini=%u ters=%d\n", g_pin, g_inv);
}

static void handleNow() {
  NowMsg m;
  for (int pi = 0; pi < g_npeers; pi++)   // her kumandadan gelen mesajlar
  while (g_peers[pi].sp->get(&m)) {
    g_peers[pi].lastRx = millis();
    if (m.type == NOW_SET && m.dev < DEV_COUNT) {
      Serial.printf("[now] SET dev=%d val=%d seq=%u\n", m.dev, m.val, m.seq);
      if (m.dev == DEV_MASA) relaySet(m.val ? 1 : 0);
      else if (m.dev == DEV_LED) g_virt_led = m.val ? 1 : 0;
      sendStateTo(pi, m.seq);
    } else if (m.type == NOW_QUERY) {
      sendStateTo(pi, m.seq);
    } else if (m.type == NOW_CFG && m.dev == DEV_MASA && g_npeers == 2 && pi == 1) {   // yalnizca telefon koprusunden
      applyRelayCfg(m.val, m.st[0] & 1);
      sendStateTo(pi, m.seq);
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

  // Kayitli role ayari (uygulamadan degistirildiyse)
  prefs.begin("nlhub", true);
  const uint8_t savedPin = prefs.getUChar("pin", 255);
  if (savedPin != 255 && GPIO_IS_VALID_OUTPUT_GPIO(savedPin) && savedPin != TOUCH_PIN) { g_pin = savedPin; g_inv = prefs.getUChar("inv", RELAY_ACTIVE_LOW) != 0; }
  prefs.end();

  // Role acilista gitmesin: cikisi acmadan once seviyeyi kapali yap
  gpio_set_level((gpio_num_t)g_pin, RELAY_OFF);
  pinMode(g_pin, OUTPUT);
  relaySet(false);

  touchCalibrate();

  SpanPoint::setPassword(NOW_PASSWORD);
  g_ctrl = new SpanPoint(NOW_CONTROLLER_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);
  g_peers[0].sp = g_ctrl;
#ifdef NOW_BRIDGE_MAC
  g_peers[1].sp = new SpanPoint(NOW_BRIDGE_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);   // telefon koprusu
  g_npeers = 2;
#endif
  homeSpan.setLogLevel(0);
  SpanPoint::setChannelMask(1 << NOW_CHANNEL);   // radyoyu sabit kanala kilitle
  Serial.printf("MASA DUGUMU  MAC = %s  sabit kanal = %d\n", Network.macAddress().c_str(), NOW_CHANNEL);
}

void loop() {
  handleNow();
  touchLoop();
  delay(5);
}
