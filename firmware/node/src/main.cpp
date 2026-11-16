#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_random.h>
#include <driver/gpio.h>
#include <Preferences.h>
#include "HomeSpan.h"
#include "now_proto.h"
#include "pair_crypto.h"

// Nowlight genel isik dugumu. Ilk acilista HICBIR ayari yok: kopruyle eslesme bekler.
//  1) ESLESME: tum kanallari tarar, kopru beacon'unu duydugu kanalda kilitlenir, HELLO yayinlar. Uygulamada kullanici secince
//     kopru ag parolasi + kanal + yuva numarasini X25519/AES-GCM ile sifreli yollar; dugum kaydeder ve yeniden baslar.
//  2) CALISMA: HomeSpan SpanPoint ile kopruyle sifreli konusur; yuvasina gelen SET komutuyla role surer.
// Role mantigi: ters (LOW = acik), elektrik gelince KAPALI baslar. BOOT dugmesine 5 sn basili tutmak eslesmeyi siler.

#ifndef NL_RELAY_PIN
#define NL_RELAY_PIN 5
#endif
#ifndef NL_BOOT_PIN
#define NL_BOOT_PIN 0
#endif
#if CONFIG_IDF_TARGET_ESP32S3
#define NL_CHIP CHIP_S3
#elif CONFIG_IDF_TARGET_ESP32C3
#define NL_CHIP CHIP_C3
#elif CONFIG_IDF_TARGET_ESP32C6
#define NL_CHIP CHIP_C6
#elif CONFIG_IDF_TARGET_ESP32S2
#define NL_CHIP CHIP_S2
#else
#define NL_CHIP CHIP_ESP32
#endif

static Preferences prefs;
static bool g_on = false;
static uint8_t g_pin = NL_RELAY_PIN;    // role pini (eslesirken uygulamadan gelir, sonradan da degistirilebilir)
static bool g_inv = true;               // ters mantik: LOW = acik

static void relaySet(bool on) { g_on = on; digitalWrite(g_pin, (on != g_inv) ? HIGH : LOW); }

static String macStr(const uint8_t *m) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

static void forgetPairing() {
  prefs.begin("nlnode", false); prefs.clear(); prefs.end();
  Serial.println("[node] eslesme silindi, yeniden baslatiliyor");
  delay(200);
  ESP.restart();
}

// ======================================================================= ESLESME
static uint8_t g_sec[32], g_pub[32];
static uint8_t g_bridgeMac[6];
static volatile uint32_t g_beaconMs = 0;
static volatile bool g_haveAccept = false;
static PairAccept g_accept;
static uint8_t g_acceptSrc[6];

static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len < 4 || !pair_magic_ok(data)) return;
  if (data[2] == PAIR_BEACON && len == (int)sizeof(PairBeacon) && data[3] == PAIR_VERSION) {
    memcpy(g_bridgeMac, info->src_addr, 6);
    g_beaconMs = millis();
  } else if (data[2] == PAIR_ACCEPT && len == (int)sizeof(PairAccept) && data[3] == PAIR_VERSION && !g_haveAccept) {
    memcpy(&g_accept, data, sizeof(PairAccept));
    memcpy(g_acceptSrc, info->src_addr, 6);
    g_haveAccept = true;
  }
}

static void pairingSetup() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_now_init();
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t pi = {};
  memset(pi.peer_addr, 0xFF, 6);
  pi.channel = 0; pi.ifidx = WIFI_IF_STA; pi.encrypt = false;
  esp_now_add_peer(&pi);
  esp_fill_random(g_sec, 32);
  x25519_public(g_pub, g_sec);
}

static bool handleAccept() {
  uint8_t me[6];
  WiFi.macAddress(me);
  if (memcmp(g_acceptSrc, g_bridgeMac, 6) != 0) return false;        // beacon'u duydugumuz koprudan gelmeli
  uint8_t shared[32], key[32];
  x25519_shared(shared, g_sec, g_accept.pub);
  pair_derive_key(key, shared, me, g_bridgeMac);
  PairSecret ps;
  if (!pair_decrypt(key, g_accept.iv, me, g_bridgeMac, g_accept.ct, g_accept.tag, ps)) { Serial.println("[eslesme] ACCEPT cozulemedi (sifre/etiket hatali), yoksayildi"); return false; }
  if (ps.channel < 1 || ps.channel > 13 || ps.slot < PAIR_FIRST_SLOT || ps.slot >= PAIR_FIRST_SLOT + PAIR_MAX_NODES) return false;
  ps.pass[sizeof(ps.pass) - 1] = 0;
  prefs.begin("nlnode", false);
  prefs.putBytes("bmac", g_bridgeMac, 6);
  prefs.putString("pass", ps.pass);
  prefs.putUChar("ch", ps.channel);
  prefs.putUChar("slot", ps.slot);
  if (ps.pin != 255 && GPIO_IS_VALID_OUTPUT_GPIO(ps.pin)) { prefs.putUChar("pin", ps.pin); }
  prefs.putUChar("inv", ps.flags & 1);
  prefs.end();
  Serial.printf("[eslesme] tamam: kopru %s kanal %u yuva %u. Yeniden baslatiliyor\n", macStr(g_bridgeMac).c_str(), ps.channel, ps.slot);
  delay(300);
  ESP.restart();
  return true;
}

static void pairingLoop() {
  static uint8_t ch = 0;
  static uint32_t dwell = 0, lastHello = 0;
  static bool locked = false;

  if (g_haveAccept) { handleAccept(); g_haveAccept = false; }

  const bool beaconFresh = g_beaconMs && millis() - g_beaconMs < 5000;
  if (!beaconFresh) {                                  // koprunun kanalini ara
    locked = false;
    if (millis() - dwell >= 400 || ch == 0) {
      ch = ch >= 13 ? 1 : ch + 1;
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      dwell = millis();
    }
  } else {
    if (!locked) { locked = true; Serial.printf("[eslesme] kopru bulundu (kanal %u), uygulamadan secilmeyi bekliyor\n", ch); }
    if (millis() - lastHello >= 700) {
      lastHello = millis();
      PairHello h = {};
      h.magic[0] = PAIR_MAGIC0; h.magic[1] = PAIR_MAGIC1; h.type = PAIR_HELLO; h.ver = PAIR_VERSION; h.kind = KIND_RELAY;
      h.chip = NL_CHIP; h.defPin = NL_RELAY_PIN; h.defInv = 1;
      uint8_t me[6]; WiFi.macAddress(me);
      snprintf(h.name, sizeof(h.name), "Nowlight-%02X%02X", me[4], me[5]);
      memcpy(h.pub, g_pub, 32);
      const uint8_t bc[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
      esp_now_send(bc, (const uint8_t *)&h, sizeof(h));
    }
  }
}

// ======================================================================= CALISMA
static SpanPoint *g_bridge = nullptr;
static uint8_t g_slot = 0;

static void sendState(uint16_t seq) {
  NowMsg r = {};
  r.type = NOW_STATE; r.dev = g_slot; r.val = g_on ? 1 : 0;
  for (int i = 0; i < 4; i++) r.st[i] = NOW_UNKNOWN;     // yerlesik isiklarin durumuna dokunma; bu dugumun durumu dev/val'de
  r.seq = seq;
  g_bridge->send(&r);
}

static void runSetup(const uint8_t *bmac, const String &pass, uint8_t ch, uint8_t slot) {
  g_slot = slot;
  SpanPoint::setPassword(pass.c_str());
  g_bridge = new SpanPoint(macStr(bmac).c_str(), sizeof(NowMsg), sizeof(NowMsg), 4);
  homeSpan.setLogLevel(0);
  SpanPoint::setChannelMask(1 << ch);
  Serial.printf("\nNOWLIGHT DUGUMU  MAC = %s  kopru = %s  kanal = %u  yuva = %u\n", Network.macAddress().c_str(), macStr(bmac).c_str(), ch, slot);
}

static void runLoop() {
  NowMsg m;
  while (g_bridge->get(&m)) {
    if (m.type == NOW_SET && m.dev == g_slot) {
      relaySet(m.val ? 1 : 0);
      Serial.printf("[now] SET %s seq=%u\n", g_on ? "ACIK" : "KAPALI", m.seq);
      sendState(m.seq);
    } else if (m.type == NOW_QUERY) {
      sendState(m.seq);
    } else if (m.type == NOW_CFG && m.dev == g_slot) {
      const uint8_t pin = m.val;
      const bool inv = m.st[0] & 1;
      if (GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
        const bool wasOn = g_on;
        if (pin != g_pin) { relaySet(false); pinMode(g_pin, INPUT); g_pin = pin; digitalWrite(g_pin, g_inv ? HIGH : LOW); pinMode(g_pin, OUTPUT); }
        g_inv = inv;
        relaySet(wasOn && pin == g_pin ? wasOn : false);
        prefs.begin("nlnode", false); prefs.putUChar("pin", g_pin); prefs.putUChar("inv", g_inv ? 1 : 0); prefs.end();
        Serial.printf("[now] CFG role pini=%u ters=%d\n", g_pin, g_inv);
      } else Serial.printf("[now] CFG reddedildi: gecersiz pin %u\n", pin);
      sendState(m.seq);
    } else if (m.type == NOW_FORGET && m.dev == g_slot) {
      forgetPairing();
    }
  }
}

// ======================================================================= ortak
static bool g_paired = false;

static void serialCommands() {
  static String line;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c != '\n' && c != '\r') { line += c; continue; }
    line.trim();
    if (line == "reset") forgetPairing();
    else if (line == "status") Serial.printf("[durum] %s MAC=%s role=%s\n", g_paired ? "eslesmis" : "eslesme bekliyor", WiFi.macAddress().c_str(), g_on ? "ACIK" : "KAPALI");
    else if (line.length()) Serial.println("komutlar: status | reset (eslesmeyi sil)");
    line = "";
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Kayitli role pini/mantigi (yoksa derleme varsayilani). Role acilista gitmesin: cikisi acmadan once KAPALI seviyeyi yaz.
  prefs.begin("nlnode", true);
  if (prefs.isKey("pin")) g_pin = prefs.getUChar("pin", NL_RELAY_PIN);
  if (prefs.isKey("inv")) g_inv = prefs.getUChar("inv", 1) != 0;
  prefs.end();
  digitalWrite(g_pin, g_inv ? HIGH : LOW);
  pinMode(g_pin, OUTPUT);
  relaySet(false);

  // Acilista BOOT 3 sn basili = eslesmeyi sil
  pinMode(NL_BOOT_PIN, INPUT_PULLUP);
  uint32_t t0 = millis();
  while (digitalRead(NL_BOOT_PIN) == LOW && millis() - t0 < 3000) delay(20);
  if (millis() - t0 >= 3000) forgetPairing();

  prefs.begin("nlnode", true);
  g_paired = prefs.isKey("bmac") && prefs.isKey("pass") && prefs.isKey("ch") && prefs.isKey("slot");
  uint8_t bmac[6] = {0}; String pass; uint8_t ch = 0, slot = 0;
  if (g_paired) { prefs.getBytes("bmac", bmac, 6); pass = prefs.getString("pass", ""); ch = prefs.getUChar("ch", 0); slot = prefs.getUChar("slot", 0); }
  prefs.end();

  if (g_paired) runSetup(bmac, pass, ch, slot);
  else { pairingSetup(); Serial.printf("\nNOWLIGHT DUGUMU  MAC = %s  ESLESME BEKLIYOR. Uygulamada Cihaz Ekle > Cihaz ekle.\n", WiFi.macAddress().c_str()); }
}

void loop() {
  serialCommands();
  // Calisirken BOOT 5 sn basili = eslesmeyi sil
  static uint32_t bootDown = 0;
  if (digitalRead(NL_BOOT_PIN) == LOW) { if (!bootDown) bootDown = millis(); else if (millis() - bootDown > 5000) forgetPairing(); } else bootDown = 0;

  if (g_paired) runLoop(); else pairingLoop();
  delay(5);
}
