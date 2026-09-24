#include "HomeSpan.h"
#include "now_proto.h"

// Yatak lambasi: ESP-NOW ile kumandadan komut alir, rolesi (GPIO5, ters mantik: LOW = acik) surer.
// WiFi'ye / router'a / HomeKit'e BAGLANMAZ. Uc cihaz NOW_CHANNEL kanalinda sabit bulusur.
// Elektrik gelince lamba her zaman KAPALI baslar.

#define RELAY_PIN 5
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

static SpanPoint *g_ctrl = nullptr;   // kumanda (komutlar / durum)
static bool g_on = false;

static void relaySet(bool on) {
  g_on = on;
  digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

static void sendState(uint16_t seq) {
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

  SpanPoint::setPassword(NOW_PASSWORD);
  g_ctrl = new SpanPoint(NOW_CONTROLLER_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);
  homeSpan.setLogLevel(0);
  SpanPoint::setChannelMask(1 << NOW_CHANNEL);   // radyoyu sabit kanala kilitle
  Serial.printf("\nYATAK DUGUMU  MAC = %s  sabit kanal = %d\n", Network.macAddress().c_str(), NOW_CHANNEL);
}

void loop() {
  NowMsg m;
  while (g_ctrl->get(&m)) {
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
