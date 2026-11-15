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
// Esik KAPALI KASAYLA (bant 1 mm duvarin arkasinda) olculdu: parmak ucu x1.09-1.11, avuc ici x1.17, yaklastirma (degmeden) x1.04'u gecmedi.
// Kasa degisirse yeniden olc (TOUCH_DEBUG=1) ve bunu ayarla.
#define TOUCH_DELTA_PCT  6.5   // dokunma: baseline'a gore % sapma. ESP32-S3'te dokununca deger YUKSELIR
#define TOUCH_READS      3     // kac ardisik okuma (15 ms arayla) esigin ustunde olmali: tek seferlik parazit sayilmasin
#define TOUCH_RELEASE_PCT 3    // birakma esigi (histerezis)
#define TOUCH_BETWEEN_MS 500   // iki tetik arasi en az bekleme (pes pese dokunma korumasi)
#define TOUCH_DEBUG      0     // 1: dokunma degerlerini seri porta yaz (esigi ayarlamak icin)
#define PUSH_QUIET_MS    25000 // kumanda bu kadar sessizse (uykuda/kapali) kendiliginden bildirim atma

static SpanPoint *g_ctrl = nullptr;   // kumanda (komutlar / durum)
static bool g_on = false;
static float g_baseline = 0;
static uint32_t g_evStart = 0;   // debug: dokunma olayinin basladigi an
// Kumandalar: [0] panel (CYD), [1] telefon koprusu (NOW_BRIDGE_MAC tanimliysa). Her biriyle ayri ESP-NOW baglantisi.
struct Peer { SpanPoint *sp; uint32_t lastRx; };
static Peer g_peers[2] = {{nullptr, 0}, {nullptr, 0}};
static int g_npeers = 1;

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

static void sendStateTo(int pi, uint16_t seq) {
  if (seq == 0 && (g_peers[pi].lastRx == 0 || millis() - g_peers[pi].lastRx > PUSH_QUIET_MS)) {   // kumanda sessiz: bosuna bekleme
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
  const boolean ok = g_peers[pi].sp->send(&r);   // kilitli kanalda kumanda uykudaysa hizla basarisiz olur, sorun degil
  Serial.printf("[now] STATE yatak=%d seq=%u -> %s\n", r.st[DEV_YATAK], seq, ok ? "ok" : "ulasilamadi");
}

// seq != 0: istegi yapan kumandaya yanit (sendStateTo); seq == 0: kendiliginden bildirim, tum kumandalara.
static void sendState(uint16_t seq) {
  for (int i = 0; i < g_npeers; i++) sendStateTo(i, seq);
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
  g_peers[0].sp = g_ctrl;
#ifdef NOW_BRIDGE_MAC
  g_peers[1].sp = new SpanPoint(NOW_BRIDGE_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);   // telefon koprusu
  g_npeers = 2;
#endif
  homeSpan.setLogLevel(0);
  SpanPoint::setChannelMask(1 << NOW_CHANNEL);   // radyoyu sabit kanala kilitle
  Serial.printf("\nYATAK DUGUMU  MAC = %s  sabit kanal = %d\n", Network.macAddress().c_str(), NOW_CHANNEL);
}

static void touchLoop() {
  static bool touched = false;
  static uint8_t strongCount = 0;
  static uint32_t lastRead = 0, holdOff = 0;

  if (millis() - lastRead < 15) return;
  lastRead = millis();
  const float v = touchRead(TOUCH_PIN);
#if TOUCH_DEBUG
  // Her dokunma/yaklasma olayinin gercek zirvesini yaz (baseline'in %4 ustuyle baslar, altina inince biter)
  static uint32_t lastDbg = 0, evStart = 0, evPeakAt = 0; static float evPeak = 0; static bool inEv = false;
  if (v > g_baseline * 1.04f) { if (!inEv) { inEv = true; evStart = millis(); g_evStart = evStart; evPeak = 0; } if (v > evPeak) { evPeak = v; evPeakAt = millis(); } }
  else if (inEv) { inEv = false; Serial.printf("[touch] olay zirve=%.0f (x%.2f) sure=%lums yukselme=%lums\n", evPeak, evPeak / g_baseline, (unsigned long)(millis() - evStart), (unsigned long)(evPeakAt - evStart)); }
  if (millis() - lastDbg > 3000) { lastDbg = millis(); Serial.printf("[touch] bosta deger=%.0f baseline=%.0f\n", v, g_baseline); }
#endif
  const bool above   = v > g_baseline * (1.0f + TOUCH_DELTA_PCT / 100.0f);
  const bool release = v < g_baseline * (1.0f + TOUCH_RELEASE_PCT / 100.0f);

  if (!touched) {
    strongCount = above ? strongCount + 1 : 0;
    if (strongCount >= TOUCH_READS && millis() > holdOff) {
      touched = true;
      holdOff = millis() + TOUCH_BETWEEN_MS;
      relaySet(!g_on);
      Serial.printf("[touch] lamba -> %s (gecikme %lu ms)\n", g_on ? "ACIK" : "KAPALI", (unsigned long)(millis() - g_evStart));
      sendState(0);
    } else if (!above) {
      g_baseline += 0.002f * (v - g_baseline);          // yavas kayma takibi (yalnizca dokunma/yaklasma yokken)
    }
  } else if (release) {
    touched = false;
    strongCount = 0;
  }
}

void loop() {
  touchLoop();
  NowMsg m;
  for (int pi = 0; pi < g_npeers; pi++)   // her kumandadan gelen mesajlar
  while (g_peers[pi].sp->get(&m)) {
    g_peers[pi].lastRx = millis();
    if (m.type == NOW_SET && m.dev == DEV_YATAK) {
      relaySet(m.val ? 1 : 0);
      Serial.printf("[now] SET yatak=%d seq=%u\n", m.val, m.seq);
      sendStateTo(pi, m.seq);
    } else if (m.type == NOW_QUERY) {
      sendStateTo(pi, m.seq);
    }
  }
  delay(5);
}
