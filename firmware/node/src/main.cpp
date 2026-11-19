#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_random.h>
#include <driver/gpio.h>
#include <Preferences.h>
#include "HomeSpan.h"
#include "now_proto.h"
#include "role_logic.h"
#include "pair_crypto.h"

// Nowlight genel isik dugumu. Ilk acilista HICBIR ayari yok: kopruyle eslesme bekler.
//  1) ESLESME: tum kanallari tarar, kopru beacon'unu duydugu kanalda kilitlenir, HELLO yayinlar. Uygulamada kullanici secince
//     kopru ag parolasi + kanal + yuva numarasini X25519/AES-GCM ile sifreli yollar; dugum kaydeder ve yeniden baslar.
//  2) CALISMA: HomeSpan SpanPoint ile kopruyle sifreli konusur; yuvasina gelen SET komutuyla role surer.
// Role mantigi: ters (LOW = acik), elektrik gelince KAPALI baslar. BOOT dugmesine 5 sn basili tutmak eslesmeyi siler.
// ROLLER (eslesmede uygulamadan secilir, flash'ta "role"): ISIK (role), SERVO (aci kontrollu, istenince BIRAKILIR: elle oynatilabilir), PIR (hareket sensoru).

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

static uint8_t g_role = ROLE_LIGHT;     // eslesmede kopru secer (PairSecret.reserved[0])

static void relaySet(bool on) { g_on = on; digitalWrite(g_pin, (on != g_inv) ? HIGH : LOW); }

// ======================================================================= SERVO
// 50 Hz PWM (LEDC). Darbe 544..2400 us = 0..180 derece. Servo yalnizca HAREKET EDERKEN surulur; hedefe varinca (birakma suresi sonra)
// PWM kesilir ve pin asagi cekilir: servo artik karsi koymaz, elle oynatilabilir (perde, vana, mekanik anahtar...).
// Konum geri bildirimi yok: bildirilen aci "son komut verilen aci"dir; servo birakilmisken elle oynatildiysa gercek konum farkli olabilir.
struct ServoCfg { uint8_t offA = 0, onA = 90, speed = 0, releaseDs = 10, minA = 0, maxA = 180; bool inv = false; };   // speed: 10 derece/sn (0 = aninda); releaseDs: 100 ms (0 = asla birak)
static ServoCfg g_sv;
static float g_svCur = 0, g_svTarget = 0, g_svSpeed = 0;   // derece, derece/sn
static bool g_svKnown = false, g_svAttached = false, g_svFree = true, g_svMoving = false;
static uint32_t g_svDoneAt = 0, g_svLastTick = 0;
static bool g_svReleasedEvent = false;      // birakildi: kopruye bildir
static bool g_svArrivedEvent = false;       // hedefe varildi (hareket bitti): kopruye bildir
static uint32_t g_svSettleUntil = 0;         // aninda hareketlerde servo mekanik olarak yerine oturana kadar (800 ms) "hareket ediyor" say

static void servoWrite(float logical) {
  ledcWrite(g_pin, (servo_pulse_us(logical, g_sv.inv) * 16384UL) / 20000UL);       // 14 bit, 20 ms periyot (darbe: role_logic.h)
}
static void servoAttach() {
  if (g_svAttached) return;
  ledcAttach(g_pin, 50, 14);
  g_svAttached = true; g_svFree = false;
}
static void servoRelease() {                        // PWM'i kes, pini asagi cek: servo gevser
  if (g_svAttached) { ledcDetach(g_pin); g_svAttached = false; }
  pinMode(g_pin, INPUT_PULLDOWN);
  g_svMoving = false; g_svFree = true;
}
static bool servoIsOn() { return fabsf(g_svTarget - g_sv.onA) <= fabsf(g_svTarget - g_sv.offA); }
static void servoSaveLast() { prefs.begin("nlnode", false); prefs.putUChar("sv_last", (uint8_t)g_svTarget); prefs.end(); }
// Hedefe git. speed: 10 derece/sn (0 = aninda, NOW_UNKNOWN = ayardaki hiz)
static void servoGoto(int angle, int speed) {
  angle = servo_clamp(angle, g_sv.minA, g_sv.maxA);
  g_svTarget = angle;
  g_on = servoIsOn();
  const int sp = (speed == NOW_UNKNOWN) ? g_sv.speed : speed;
  g_svSpeed = sp * 10.0f;
  servoAttach();
  if (!g_svKnown || sp == 0) { g_svCur = g_svTarget; g_svKnown = true; servoWrite(g_svCur); }   // konum bilinmiyor/aninda: dogrudan git
  g_svMoving = (g_svCur != g_svTarget);
  g_svDoneAt = g_svMoving ? 0 : millis();
  if (!g_svMoving) g_svSettleUntil = millis() + 800;                       // aninda: servo yine de yerine gidene kadar sure ver
  g_svLastTick = millis();
  Serial.printf("[servo] hedef %d derece (hiz %d)\n", angle, sp);
}
static void servoTick() {
  const uint32_t now = millis();
  if (g_svMoving && now - g_svLastTick >= 20) {
    const float dt = (now - g_svLastTick) / 1000.0f; g_svLastTick = now;
    g_svCur = servo_step(g_svCur, g_svTarget, g_svSpeed, dt);
    if (g_svCur == g_svTarget) { g_svMoving = false; g_svDoneAt = now; servoSaveLast(); g_svArrivedEvent = true; }
    servoWrite(g_svCur);
  } else if (g_svSettleUntil && !g_svMoving && now >= g_svSettleUntil) {
    g_svSettleUntil = 0; g_svArrivedEvent = true;
  } else if (!g_svMoving && g_svAttached && servo_should_release(g_sv.releaseDs, now, g_svDoneAt)) {
    servoRelease();
    Serial.println("[servo] birakildi (serbest)");
    g_svReleasedEvent = true;
  }
}

// ======================================================================= PIR
// Yazilim filtresi: onay suresinden kisa darbeler yok sayilir, hareket bittikten sonra "tutma" suresi kadar hareket var kalir
// (cihazin kendi potansiyometre sureleriyle ayri), temizlenince "bekleme" suresi boyunca yeniden tetiklenmez.
// Algilama ACISI/MENZILI cihazin lensi ve potansiyometresidir: yazilimdan degismez.
static PirCfg g_pir;                      // ayarlar ve filtre mantigi: role_logic.h
static PirState g_pirSt;
static bool g_pirEvent = false;           // durum degisti: kopruye bildir
static void pirTick() {
  if (pir_update(g_pir, g_pirSt, digitalRead(g_pin) == HIGH, millis())) {
    g_pirEvent = true;
    Serial.println(g_pirSt.motion ? "[pir] HAREKET" : "[pir] hareket bitti");
  }
}

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
static volatile int8_t g_rssiCh[14];          // kanal basina duyulan en guclu beacon sinyali (-127: duyulmadi)
static volatile bool g_haveAccept = false;
static PairAccept g_accept;
static uint8_t g_acceptSrc[6];

static void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len < 4 || !pair_magic_ok(data)) return;
  if (data[2] == PAIR_BEACON && len == (int)sizeof(PairBeacon) && data[3] == PAIR_VERSION) {
    memcpy(g_bridgeMac, info->src_addr, 6);
    g_beaconMs = millis();
    const int ch = info->rx_ctrl ? info->rx_ctrl->channel : 0;
    const int8_t rssi = info->rx_ctrl ? info->rx_ctrl->rssi : -100;
    if (ch >= 1 && ch <= 13 && rssi > g_rssiCh[ch]) g_rssiCh[ch] = rssi;
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
  for (int i = 0; i < 14; i++) g_rssiCh[i] = -127;
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
  prefs.putUChar("role", ps.reserved[0] <= ROLE_PIR ? ps.reserved[0] : ROLE_LIGHT);   // eski kopru 0 yollar = isik
  prefs.end();
  Serial.printf("[eslesme] tamam: kopru %s kanal %u yuva %u. Yeniden baslatiliyor\n", macStr(g_bridgeMac).c_str(), ps.channel, ps.slot);
  delay(300);
  ESP.restart();
  return true;
}

static void pairingLoop() {
  static uint8_t ch = 0;
  static uint32_t dwell = 0, lastHello = 0, lockedAt = 0;
  static bool locked = false;
  static bool tried[14];                               // bu turda denenen kanallar

  if (g_haveAccept) { handleAccept(); g_haveAccept = false; }

  if (locked) {
    if (!(g_beaconMs && millis() - g_beaconMs < 6000)) {        // kopru sustu (pencere kapandi): yeniden tara
      locked = false; ch = 0;
      for (int i = 0; i < 14; i++) { g_rssiCh[i] = -127; tried[i] = false; }
      Serial.println("[eslesme] kopru sesi kesildi, yeniden taraniyor");
    } else if (millis() - lockedAt > 20000) {
      // 20 sn'dir eslesme olmadi: bu kanal (komsu kanalin yankisi olabilir) kotu secim, sinyal siralamasinda bir sonrakine gec
      tried[ch] = true;
      uint8_t next = 0; int8_t nextR = -127;
      for (uint8_t c = 1; c <= 13; c++) if (!tried[c] && g_rssiCh[c] > nextR) { nextR = g_rssiCh[c]; next = c; }
      if (next) {
        esp_wifi_set_channel(next, WIFI_SECOND_CHAN_NONE);
        ch = next; lockedAt = millis(); lastHello = 0;
        Serial.printf("[eslesme] eslesme olmadi, siradaki kanal: %u (sinyal %d dBm)\n", next, nextR);
      } else {                                         // hepsi denendi: bastan tara
        locked = false; ch = 0;
        for (int i = 0; i < 14; i++) { g_rssiCh[i] = -127; tried[i] = false; }
        Serial.println("[eslesme] tum kanallar denendi, yeniden taraniyor");
      }
      return;
    } else {
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
      return;
    }
  }

  // Tarama: 1..13 her kanalda 350 ms dinle, beacon sinyallerini kaydet. Komsu kanallar (orn. 5 ve 7) da koprunun 6'daki sinyalini
  // zayifca duyar; tam tarama bitince EN GUCLU kanala kilitlen (ilk duyulana degil).
  if (ch == 0 || millis() - dwell >= 350) {
    if (ch == 13) {
      uint8_t best = 0; int8_t bestR = -127;
      for (uint8_t c = 1; c <= 13; c++) if (g_rssiCh[c] > bestR) { bestR = g_rssiCh[c]; best = c; }
      if (best) {
        esp_wifi_set_channel(best, WIFI_SECOND_CHAN_NONE);
        locked = true; ch = best; lastHello = 0; lockedAt = millis(); g_beaconMs = millis();
        for (int i = 0; i < 14; i++) tried[i] = false;
        Serial.printf("[eslesme] kopru bulundu: kanal %u (sinyal %d dBm), uygulamadan secilmeyi bekliyor\n", best, bestR);
        return;
      }
      ch = 0;                                         // duyulmadi: taramaya bastan basla
    }
    ch = ch >= 13 ? 1 : ch + 1;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    dwell = millis();
  }
}

// ======================================================================= CALISMA
static SpanPoint *g_bridge = nullptr;
static uint8_t g_slot = 0;

static void sendState(uint16_t seq) {
  NowMsg r = {};
  r.type = NOW_STATE; r.dev = g_slot; r.val = (g_role == ROLE_PIR ? g_pirSt.motion : g_on) ? 1 : 0;
  for (int i = 0; i < 4; i++) r.st[i] = NOW_UNKNOWN;     // yerlesik isiklarin durumuna dokunma; bu dugumun durumu dev/val'de
  r.seq = seq;
  g_bridge->send(&r);
}

// Servo: son komut verilen aci ve serbest/tutuyor bilgisi (seq: yanit verdigimiz komutun numarasi, kendiliginden ise 0)
static void sendAngle(uint16_t seq) {
  NowMsg r = {};
  r.type = NOW_ANGLE; r.dev = g_slot; r.val = (uint8_t)g_svTarget;
  r.st[0] = (g_svFree ? 1 : 0) | ((g_svMoving || g_svSettleUntil) ? 2 : 0);   // bit0 serbest, bit1 hareket ediyor
  r.st[1] = (uint8_t)g_svCur; r.st[2] = r.st[3] = NOW_UNKNOWN;                 // st[1]: su anki aci
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

static void saveServoCfg() {
  prefs.begin("nlnode", false);
  prefs.putUChar("pin", g_pin);
  prefs.putUChar("sv_inv", g_sv.inv ? 1 : 0); prefs.putUChar("sv_off", g_sv.offA); prefs.putUChar("sv_on", g_sv.onA);
  prefs.putUChar("sv_spd", g_sv.speed); prefs.putUChar("sv_rel", g_sv.releaseDs); prefs.putUChar("sv_min", g_sv.minA); prefs.putUChar("sv_max", g_sv.maxA);
  prefs.end();
}
static void savePirCfg() {
  prefs.begin("nlnode", false);
  prefs.putUChar("pin", g_pin);
  prefs.putUChar("pr_hold", g_pir.holdS); prefs.putUChar("pr_conf", g_pir.confirm10); prefs.putUChar("pr_cool", g_pir.cooldownS); prefs.putUChar("pr_inv", g_pir.inv ? 1 : 0);
  prefs.end();
}

static void runLoop() {
  NowMsg m;
  if (g_role == ROLE_SERVO) {
    servoTick();
    if (g_svReleasedEvent) { g_svReleasedEvent = false; sendAngle(0); }
    if (g_svArrivedEvent) { g_svArrivedEvent = false; sendAngle(0); }
  } else if (g_role == ROLE_PIR) {
    pirTick();
    if (g_pirEvent) { g_pirEvent = false; sendState(0); }
  }
  while (g_bridge->get(&m)) {
    if (g_role == ROLE_SERVO && m.dev == g_slot && m.type == NOW_SET) {           // acik/kapali = ayarli iki aci
      servoGoto(m.val ? g_sv.onA : g_sv.offA, NOW_UNKNOWN);
      sendState(m.seq); sendAngle(m.seq);
    } else if (g_role == ROLE_SERVO && m.dev == g_slot && m.type == NOW_SERVO_GOTO) {   // serbest aci (kaydirici, otomasyon)
      servoGoto(m.val, m.st[0]);
      sendState(m.seq); sendAngle(m.seq);
    } else if (g_role == ROLE_SERVO && m.dev == g_slot && m.type == NOW_SERVO_FREE) {   // hemen birak
      servoRelease();
      sendAngle(m.seq);
    } else if (g_role == ROLE_SERVO && m.dev == g_slot && m.type == NOW_SERVO_CFG) {
      if (GPIO_IS_VALID_OUTPUT_GPIO(m.val)) {
        if (m.val != g_pin) { servoRelease(); pinMode(g_pin, INPUT); g_pin = m.val; pinMode(g_pin, INPUT_PULLDOWN); }
        g_sv.inv = m.st[0] & 1; g_sv.offA = m.st[1] > 180 ? 180 : m.st[1]; g_sv.onA = m.st[2] > 180 ? 180 : m.st[2]; g_sv.speed = m.st[3];
        saveServoCfg();
        Serial.printf("[now] SERVO_CFG pin=%u kapali=%u acik=%u hiz=%u ters=%d\n", g_pin, g_sv.offA, g_sv.onA, g_sv.speed, g_sv.inv);
      } else Serial.printf("[now] SERVO_CFG reddedildi: gecersiz pin %u\n", m.val);
    } else if (g_role == ROLE_SERVO && m.dev == g_slot && m.type == NOW_SERVO_CFG2) {
      g_sv.releaseDs = m.val;
      g_sv.minA = m.st[0] > 180 ? 0 : m.st[0]; g_sv.maxA = m.st[1] > 180 ? 180 : m.st[1];
      if (g_sv.minA > g_sv.maxA) { g_sv.minA = 0; g_sv.maxA = 180; }
      saveServoCfg();
      Serial.printf("[now] SERVO_CFG2 birakma=%u00ms sinir=%u..%u\n", g_sv.releaseDs, g_sv.minA, g_sv.maxA);
    } else if (g_role == ROLE_PIR && m.dev == g_slot && m.type == NOW_PIR_CFG) {
      if (GPIO_IS_VALID_GPIO(m.val)) {
        if (m.val != g_pin) { g_pin = m.val; pinMode(g_pin, INPUT_PULLDOWN); }
        g_pir.holdS = m.st[0]; g_pir.confirm10 = m.st[1]; g_pir.cooldownS = m.st[2]; g_pir.inv = m.st[3] & 1;
        savePirCfg();
        Serial.printf("[now] PIR_CFG pin=%u tutma=%us onay=%ums bekleme=%us ters=%d\n", g_pin, g_pir.holdS, g_pir.confirm10 * 10, g_pir.cooldownS, g_pir.inv);
      }
    } else if (g_role == ROLE_LIGHT && m.type == NOW_SET && m.dev == g_slot) {
      relaySet(m.val ? 1 : 0);
      Serial.printf("[now] SET %s seq=%u\n", g_on ? "ACIK" : "KAPALI", m.seq);
      sendState(m.seq);
    } else if (m.type == NOW_QUERY) {
      sendState(m.seq);
      if (g_role == ROLE_SERVO) sendAngle(m.seq);
    } else if (g_role == ROLE_LIGHT && m.type == NOW_CFG && m.dev == g_slot) {
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
    else if (line == "status") Serial.printf("[durum] %s MAC=%s rol=%s durum=%s\n", g_paired ? "eslesmis" : "eslesme bekliyor", WiFi.macAddress().c_str(), g_role == ROLE_SERVO ? "servo" : g_role == ROLE_PIR ? "pir" : "isik", g_role == ROLE_PIR ? (g_pirSt.motion ? "HAREKET" : "yok") : (g_on ? "ACIK" : "KAPALI"));
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
  g_role = prefs.getUChar("role", ROLE_LIGHT);
  if (g_role > ROLE_PIR) g_role = ROLE_LIGHT;
  if (g_role == ROLE_SERVO) {
    g_sv.inv = prefs.getUChar("sv_inv", 0) != 0; g_sv.offA = prefs.getUChar("sv_off", 0); g_sv.onA = prefs.getUChar("sv_on", 90);
    g_sv.speed = prefs.getUChar("sv_spd", 0); g_sv.releaseDs = prefs.getUChar("sv_rel", 10); g_sv.minA = prefs.getUChar("sv_min", 0); g_sv.maxA = prefs.getUChar("sv_max", 180);
    g_svTarget = g_svCur = prefs.getUChar("sv_last", g_sv.offA);
    g_on = servoIsOn();
  } else if (g_role == ROLE_PIR) {
    g_pir.holdS = prefs.getUChar("pr_hold", 5); g_pir.confirm10 = prefs.getUChar("pr_conf", 5); g_pir.cooldownS = prefs.getUChar("pr_cool", 0); g_pir.inv = prefs.getUChar("pr_inv", 0) != 0;
  }
  prefs.end();
  if (g_role == ROLE_LIGHT) {
    gpio_set_level((gpio_num_t)g_pin, g_inv ? 1 : 0);   // once seviyeyi yaz, sonra cikis yap: role acilista tikirdamasin
    pinMode(g_pin, OUTPUT);
    relaySet(false);
  } else {
    pinMode(g_pin, INPUT_PULLDOWN);                     // servo: ilk komuta kadar SURULMEZ (acilista sicramaz); PIR: giris
  }

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
