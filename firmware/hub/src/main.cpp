#include "HomeSpan.h"
#include "now_proto.h"
#include "esp_wifi.h"

// Masa lambasi hub'i (ESP32-S3 Super Mini):
//   - Apple Home lambasi (HomeSpan)
//   - kapasitif dokunma bandi ile ac/kapa
//   - role surme
//   - ESP-NOW (SpanPoint) ile kumanda ile iki yonlu haberlesme
// Hub'in WiFi'ye bagli olmasi gerekir: ESP-NOW kanali, hub'in router'a bagli oldugu kanaldir.

// ---- Donanim ayarlari (kendi kablolamana gore degistir) ----
#define DEVICE_NAME        "Masa Lambasi"
#define RELAY_PIN          5
#define RELAY_ACTIVE_LOW   1      // 1: IN=LOW iken role calisir (cogu 5V role modulu), 0: IN=HIGH
#define TOUCH_PIN          4      // ESP32-S3'te GPIO1..14 dokunmatiktir (T1..T14)

// Dokununca okuma degeri ESP32-S3'te YUKSELIR, klasik ESP32'de DUSER.
#if CONFIG_IDF_TARGET_ESP32S3
  #define TOUCH_RISES_ON_TOUCH 1
#else
  #define TOUCH_RISES_ON_TOUCH 0
#endif
#define TOUCH_DELTA_PCT    20     // baseline'a gore % sapma = dokunma (bant boyutuna gore ayarla)
#define TOUCH_DEBUG        0      // 1: dokunma degerlerini seri porta yaz (esigi ayarlamak icin)

#define PUSH_QUIET_MS      25000  // kumanda bu kadar sessizse (uykuda) kendiliginden bildirim atma

#define RELAY_ON_LEVEL     (RELAY_ACTIVE_LOW ? LOW : HIGH)
#define RELAY_OFF_LEVEL    (RELAY_ACTIVE_LOW ? HIGH : LOW)

static float g_baseline = 0;
static SpanPoint *g_ctrl = nullptr;
static uint32_t g_lastCtrlRx = 0;   // kumandadan son mesaj
static uint8_t g_virt[DEV_COUNT] = {0, 0, 0};   // LED: henuz gercek cihaz yok, sadece durum tutulur

static void relaySet(bool on) {
  digitalWrite(RELAY_PIN, on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

static void touchCalibrate() {
  Serial.println("\n=== TOUCH KALIBRASYONU: 3 sn dokunma ===");
  delay(3000);
  uint32_t sum = 0;
  for (int i = 0; i < 40; i++) { sum += touchRead(TOUCH_PIN); delay(10); }
  g_baseline = sum / 40.0f;
  Serial.printf("Baseline: %.0f\n", g_baseline);
}

static bool touchActive(float v) {
  const float d = g_baseline * TOUCH_DELTA_PCT / 100.0f;
  return TOUCH_RISES_ON_TOUCH ? (v > g_baseline + d) : (v < g_baseline - d);
}

static bool touchReleased(float v) {           // birakma esigi baslama esiginin yarisi (histerezis)
  const float d = g_baseline * TOUCH_DELTA_PCT / 200.0f;
  return TOUCH_RISES_ON_TOUCH ? (v < g_baseline + d) : (v > g_baseline - d);
}

// Kumandaya tum cihazlarin durumunu gonderir (seq: yanit verdigimiz istegin numarasi, kendiliginden ise 0)
static void sendState(bool masaOn, uint16_t seq) {
  NowMsg r = {};
  r.type = NOW_STATE;
  r.st[DEV_MASA]  = masaOn ? 1 : 0;
  r.st[DEV_YATAK] = NOW_UNKNOWN;         // yatak lambasi kendi dugumunden yonetilir
  r.st[DEV_LED]   = g_virt[DEV_LED];
  r.st[3]         = NOW_UNKNOWN;
  r.seq = seq;
  if (seq == 0 && (g_lastCtrlRx == 0 || millis() - g_lastCtrlRx > PUSH_QUIET_MS)) {   // kumanda uykuda: bosuna bekleme
    Serial.println("[now] kumanda sessiz, kendiliginden bildirim atlandi");
    return;
  }
  const boolean ok = g_ctrl->send(&r);
  Serial.printf("[now] STATE gonderildi masa=%d led=%d seq=%u -> %s\n", r.st[0], r.st[2], seq, ok ? "ok" : "ulasilamadi");
}

struct MasaLamba : Service::LightBulb {
  SpanCharacteristic *power;
  bool touched = false;
  uint8_t activeCount = 0;
  uint32_t lastRead = 0, holdOff = 0;

  MasaLamba() : Service::LightBulb() {
    power = new Characteristic::On(0);
  }

  boolean update() override {          // Apple Home'dan geldi
    const bool on = power->getNewVal();
    relaySet(on);
    Serial.printf("[home] lamba -> %s\n", on ? "ACIK" : "KAPALI");
    sendState(on, 0);
    return true;
  }

  void handleNow() {
    NowMsg m;
    while (g_ctrl->get(&m)) {
      g_lastCtrlRx = millis();
      if (m.type == NOW_SET && m.dev < DEV_COUNT) {
        Serial.printf("[now] SET dev=%d val=%d seq=%u\n", m.dev, m.val, m.seq);
        if (m.dev == DEV_MASA) {
          power->setVal(m.val ? 1 : 0);   // Apple Home'a da bildirir
          relaySet(m.val);
        } else if (m.dev == DEV_LED) {
          g_virt[m.dev] = m.val ? 1 : 0;
        }
        sendState(power->getVal(), m.seq);
      } else if (m.type == NOW_QUERY) {
        sendState(power->getVal(), m.seq);
      }
    }
  }

  void loop() override {
    handleNow();

    if (millis() - lastRead < 30) return;
    lastRead = millis();
    const float v = touchRead(TOUCH_PIN);
#if TOUCH_DEBUG
    static uint32_t lastDbg = 0;
    if (millis() - lastDbg > 500) { lastDbg = millis(); Serial.printf("[touch] deger=%.0f baseline=%.0f\n", v, g_baseline); }
#endif

    if (!touched) {
      if (touchActive(v)) {
        if (++activeCount >= 3 && millis() > holdOff) {     // 3 ardisik okuma: parazit filtresi
          touched = true;
          holdOff = millis() + 400;
          const bool nv = !power->getVal();
          power->setVal(nv);            // Apple Home'a da bildirir
          relaySet(nv);
          Serial.printf("[touch] lamba -> %s\n", nv ? "ACIK" : "KAPALI");
          sendState(nv, 0);
        }
      } else {
        activeCount = 0;
        g_baseline += 0.002f * (v - g_baseline);   // yavas kayma takibi
      }
    } else if (touchReleased(v)) {
      touched = false;
      activeCount = 0;
    }
  }
};

// WiFi her baglandiginda: guc tasarrufunu kapat (hub prizde, radyo kisa sure kapaninca tepki dalgalaniyordu)
// ve hangi aga, hangi kanala, hangi sinyalle baglandigimizi yaz (ESP-NOW kanali bu kanal olmali).
static void onConnected(int) {
  esp_wifi_set_ps(WIFI_PS_NONE);
  wifi_ap_record_t ap;
  if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
    Serial.printf("[wifi] baglandi: '%s' kanal %d RSSI %d dBm (uyku kapali)\n", (const char *)ap.ssid, ap.primary, ap.rssi);
  }
}

void setup() {
  Serial.begin(115200);

  // Role acilista gitmesin: cikisi acmadan once seviyeyi "kapali" yap
  gpio_set_level((gpio_num_t)RELAY_PIN, RELAY_OFF_LEVEL);
  pinMode(RELAY_PIN, OUTPUT);
  relaySet(false);

  touchCalibrate();

  homeSpan.setConnectionCallback(onConnected);
  homeSpan.begin(Category::Lighting, DEVICE_NAME);

  // ESP-NOW: kumanda ile haberlesme (kanali Home'a bagli olan bu cihaza gore SpanPoint ayarlar)
  SpanPoint::setPassword(NOW_PASSWORD);
  g_ctrl = new SpanPoint(NOW_CONTROLLER_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name(DEVICE_NAME);
    new MasaLamba();
}

void loop() {
  homeSpan.poll();
}
