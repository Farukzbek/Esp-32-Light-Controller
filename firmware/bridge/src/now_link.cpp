#include "HomeSpan.h"
#include "now_proto.h"
#include "now_link.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include <Preferences.h>
#include "pairing.h"
#include <string.h>

// Kontrolcu iki cihazla konusur:
//   0 = masa dugumu:       DEV_MASA, DEV_LED
//   1 = yatak ESP'si:      DEV_YATAK
// SpanPoint::send() true donse bile karsi uygulama mesaji anlamis olmayabilir (802.11 donanim ACK'i
// yazilimdan bagimsiz gelir). Gercek onay, karsinin ayni seq ile dondurdugu STATE mesajidir.

// Baglantilar: 0 = hub (masa, led), 1 = yatak, 2.. = eslesen dugumler (yuva 3..6)
#define LINKS (2 + NOW_DYN_MAX)
#ifndef NOW_VERBOSE
#define NOW_VERBOSE 0   // 1: her mesaji seri porta yaz (teshis)
#endif
#ifndef NOW_RSSI
#define NOW_RSSI 0      // 1: promiscuous dinleme ile sinyal gucu olc (teshis; radyoyu ve islemciyi yorar)
#endif

struct Link {
  SpanPoint *sp = nullptr;
  const char *name = "";
  NowMsg pending = {};
  bool havePending = false;
  uint32_t sentAt = 0;
  int tries = 0;
  bool everSeen = false;
  uint32_t lastRxMs = 0;
  uint32_t lastFailMs = 0;   // son ulasilamayan komut
  uint32_t createdMs = 0;    // eslesen dugum: baglanti olusturuldugu an
};

struct Out { NowMsg m; uint8_t link; };

static Link s_link[LINKS];

// --- Sinyal gucu (RSSI): promiscuous modda iki cihazin gonderdigi ESP-NOW cercevelerinden okunur ---
static bool s_promisc = false;      // promiscuous (RSSI) modu acik mi
static uint8_t s_mac[LINKS][6];
static volatile int8_t s_rssi[LINKS] = {};
static volatile uint32_t s_rssiMs[LINKS] = {};
static volatile int8_t s_dyn[NOW_DYN_MAX] = {-1, -1, -1, -1};   // eslesen dugumlerin durumu
static volatile int16_t s_angle[NOW_DYN_MAX] = {-1, -1, -1, -1};  // servo dugumlerinin son bildirilen acisi
static volatile uint8_t s_free[NOW_DYN_MAX] = {};                // servo birakilmis mi
static uint8_t s_role[NOW_DYN_MAX] = {};                          // eslesen dugumun rolu (ROLE_*)

static void promiscCb(void *buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t *p = (const wifi_promiscuous_pkt_t *)buf;
  if (type == WIFI_PKT_MGMT) pair_sniff(p);       // eslesme penceresi: HELLO yakala
  if (p->rx_ctrl.sig_len < 16) return;
  const uint8_t *src = p->payload + 10;            // 802.11 basligi: addr2 = gonderen
  for (int k = 0; k < LINKS; k++) {
    if (memcmp(src, s_mac[k], 6) == 0) { s_rssi[k] = p->rx_ctrl.rssi; s_rssiMs[k] = millis(); }
  }
}

static void parseMac(const char *str, uint8_t out[6]) {
  unsigned v[6] = {0};
  sscanf(str, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)v[i];
}

int8_t now_rssi(int link, uint32_t *ageMs) {
  if (link < 0 || link >= LINKS || s_rssiMs[link] == 0) { if (ageMs) *ageMs = 0xFFFFFFFF; return 0; }
  if (ageMs) *ageMs = millis() - s_rssiMs[link];
  return s_rssi[link];
}
static QueueHandle_t s_q = nullptr;
static volatile uint8_t s_state[4] = {NOW_UNKNOWN, NOW_UNKNOWN, NOW_UNKNOWN, NOW_UNKNOWN}; // henuz bildirilmemis = bilinmiyor
static volatile bool s_newState = false;
static volatile bool s_failed = false;
static volatile uint16_t s_seq = 0;

static uint8_t linkOfDev(uint8_t dev) {
  if (dev >= NOW_DYN_FIRST && dev < NOW_DYN_FIRST + NOW_DYN_MAX) return 2 + (dev - NOW_DYN_FIRST);
  return dev == DEV_YATAK ? 1 : 0;
}

static void enqueue(const NowMsg &m, uint8_t link) {
  if (link >= LINKS || !s_link[link].sp) return;   // bos yuva
  Out o = {m, link};
  if (s_q) xQueueSend(s_q, &o, 0);
}

// --- Sabit kanal ---
// Uc cihaz (kumanda, masa dugumu, yatak dugumu) NOW_CHANNEL kanalinda sabit bulusur; router/WiFi'ye bagimli degil.
// SpanPoint::send() yanit vermeyen bir cihaz icin normalde TUM kanallari tarar (radyoyu koparir, NVS'yi yipratir).
// Radyoyu tek kanala kilitleyince yanit vermeyen cihaz sadece hizla "basarisiz" olur.
static void applyLock(uint8_t ch) {
  SpanPoint::setChannelMask(1 << ch);   // radyoyu da bu kanala alir
}

static void pollIncoming() {
  for (int k = 0; k < LINKS; k++) {
    Link &L = s_link[k];
    if (!L.sp) continue;
    NowMsg in;
    while (L.sp->get(&in)) {
      if (in.type == NOW_ANGLE) {                  // servo dugumu: son aci ve serbest/tutuyor bilgisi
        if (k >= 2) { s_angle[k - 2] = in.val; s_free[k - 2] = in.st[0] & 1; }
        L.everSeen = true;
        L.lastRxMs = millis();
        if (L.havePending && in.seq == L.pending.seq) L.havePending = false;
        continue;
      }
      if (in.type != NOW_STATE) continue;
      for (int i = 0; i < 4; i++) if (in.st[i] != NOW_UNKNOWN) s_state[i] = in.st[i];
      if (k >= 2) s_dyn[k - 2] = in.val ? 1 : 0;   // eslesen dugum: durum dev/val alaninda
      s_newState = true;
      L.everSeen = true;
      L.lastRxMs = millis();
      if (L.havePending && in.seq == L.pending.seq) L.havePending = false; // komutumuz ulasti
#if NOW_VERBOSE
      Serial.printf("[now:%s] STATE masa=%d yatak=%d led=%d seq=%u\n", L.name, in.st[0], in.st[1], in.st[2], in.seq);
#endif
    }
  }
}

static void nowTask(void *) {
  now_query();   // acilista durumu hemen sor
  uint32_t lastQuery = millis();

  for (;;) {
    pollIncoming();

    // gidenler
    Out o;
    if (xQueueReceive(s_q, &o, pdMS_TO_TICKS(20)) == pdTRUE) {
      Link &L = s_link[o.link];
      if (!L.sp) continue;
      uint32_t t0 = millis();
      bool ok = L.sp->send(&o.m);
#if NOW_VERBOSE
      Serial.printf("[now:%s] gonderildi type=%d dev=%d val=%d seq=%u (radyo onayi: %s, %lu ms)\n", L.name, o.m.type, o.m.dev, o.m.val, o.m.seq, ok ? "ok" : "yok", (unsigned long)(millis() - t0));
#else
      (void)ok; (void)t0;
#endif
      if (o.m.type == NOW_SET || o.m.type == NOW_SERVO_GOTO) { L.pending = o.m; L.havePending = true; L.sentAt = millis(); L.tries = 1; }
    }

    // yanit gelmeyen komutlari yeniden dene
    for (int k = 0; k < LINKS; k++) {
      Link &L = s_link[k];
      if (!L.sp || !L.havePending || millis() - L.sentAt <= 350) continue;
      if (L.tries < 3) {
        L.sp->send(&L.pending);
        L.sentAt = millis();
        L.tries++;
        Serial.printf("[now:%s] yanit yok, yeniden deneniyor (%d) seq=%u\n", L.name, L.tries, L.pending.seq);
      } else {
        L.havePending = false;
        s_failed = true;
        L.lastFailMs = millis();
        Serial.printf("[now:%s] SET ULASAMADI seq=%u\n", L.name, L.pending.seq);
      }
    }

    // periyodik durum sorgusu (ayni zamanda baglanti kontrolu)
    if (millis() - lastQuery > 10000) {
      lastQuery = millis();
      now_query();
#if NOW_RSSI
      { uint32_t ah, ay; int8_t rh = now_rssi(0, &ah), ry = now_rssi(1, &ay);
        Serial.printf("[rssi] hub %d dBm (%lu ms once)  yatak %d dBm (%lu ms once)\n", rh, (unsigned long)ah, ry, (unsigned long)ay); }
#endif
    }
  }
}

static String macToStr(const uint8_t m[6]) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

bool now_dyn_add(uint8_t slot, const uint8_t mac[6]) {
  if (slot < NOW_DYN_FIRST || slot >= NOW_DYN_FIRST + NOW_DYN_MAX) return false;
  const int k = 2 + (slot - NOW_DYN_FIRST);
  Link n;                                    // yeni baglanti (eski varsa SpanPoint'i bellekte kalir, kullanilmaz)
  n.name = "dugum";
  n.createdMs = millis();
  memcpy(s_mac[k], mac, 6);
  n.sp = new SpanPoint(macToStr(mac).c_str(), sizeof(NowMsg), sizeof(NowMsg), 4);
  s_dyn[k - 2] = -1;
  s_angle[k - 2] = -1; s_free[k - 2] = 0;
  s_link[k] = n;
  Preferences p;
  if (p.begin("nodes", false)) { char key[4]; snprintf(key, sizeof(key), "m%u", slot); p.putBytes(key, mac, 6); p.end(); }
  Serial.printf("[now] dugum baglandi: yuva %u = %s\n", slot, macToStr(mac).c_str());
  return true;
}

bool now_dyn_present(uint8_t slot) {
  return slot >= NOW_DYN_FIRST && slot < NOW_DYN_FIRST + NOW_DYN_MAX && s_link[2 + slot - NOW_DYN_FIRST].sp != nullptr;
}
int8_t now_dyn_state(uint8_t slot) { return now_dyn_present(slot) ? s_dyn[slot - NOW_DYN_FIRST] : -1; }
bool now_dyn_linked(uint8_t slot) {
  if (!now_dyn_present(slot)) return false;
  return (now_link_down_mask() & (1 << (2 + slot - NOW_DYN_FIRST))) == 0;
}

String now_net_key() {
#ifdef NOW_GENERIC
  static String key;
  if (!key.length()) {
    Preferences p;
    p.begin("bridge", false);
    key = p.isKey("netkey") ? p.getString("netkey", "") : String();
    if (key.length() < 16) {                                   // ilk acilis: rastgele 24 karakter (PairSecret.pass 31 karaktere kadar tasir)
      char b[25];
      for (int i = 0; i < 24; i += 8) snprintf(b + i, 9, "%08x", (unsigned)esp_random());
      key = String(b);
      p.putString("netkey", key);
    }
    p.end();
  }
  return key;
#else
  return String(NOW_PASSWORD);
#endif
}

void now_init(void) {
  SpanPoint::setPassword(now_net_key().c_str());
#ifndef NOW_GENERIC
  s_link[0].name = "hub";
  s_link[0].sp = new SpanPoint(NOW_HUB_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);
  s_link[1].name = "yatak";
  s_link[1].sp = new SpanPoint(NOW_BED_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);
  parseMac(NOW_HUB_MAC, s_mac[0]);
  parseMac(NOW_BED_MAC, s_mac[1]);
#endif
  s_q = xQueueCreate(12, sizeof(Out));
  wifi_promiscuous_filter_t filt = {};
  filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(promiscCb);   // acmak icin now_rssi_enable(true)
#ifndef NOW_GENERIC
  applyLock(NOW_CHANNEL);                      // kisisel derleme: sabit kanal. Genel kopruda kanal router'i izler, dugumler tarayip bulur.
#endif
  { // kayitli eslesen dugumleri geri yukle
    Preferences p;
    if (p.begin("nodes", true)) {
      for (uint8_t slot = NOW_DYN_FIRST; slot < NOW_DYN_FIRST + NOW_DYN_MAX; slot++) {
        char key[4]; snprintf(key, sizeof(key), "m%u", slot);
        uint8_t mac[6];
        if (p.isKey(key) && p.getBytes(key, mac, 6) == 6) { p.end(); now_dyn_add(slot, mac); p.begin("nodes", true); }
        char rk[4]; snprintf(rk, sizeof(rk), "r%u", slot);
        if (p.isKey(rk)) s_role[slot - NOW_DYN_FIRST] = p.getUChar(rk, ROLE_LIGHT);
      }
      p.end();
    }
  }
#ifdef NOW_GENERIC
  Serial.printf("[now] genel kopru MAC = %s  (kanal router'i izler)\n", Network.macAddress().c_str());
#else
  Serial.printf("[now] kumanda MAC = %s  masa = %s  yatak = %s  sabit kanal = %d\n", Network.macAddress().c_str(), NOW_HUB_MAC, NOW_BED_MAC, NOW_CHANNEL);
#endif
  xTaskCreate(nowTask, "now", 8192, nullptr, 1, nullptr);
}

uint8_t now_role(uint8_t slot) {
  return (slot >= NOW_DYN_FIRST && slot < NOW_DYN_FIRST + NOW_DYN_MAX) ? s_role[slot - NOW_DYN_FIRST] : (uint8_t)ROLE_LIGHT;
}
void now_set_role(uint8_t slot, uint8_t role) {
  if (slot < NOW_DYN_FIRST || slot >= NOW_DYN_FIRST + NOW_DYN_MAX) return;
  s_role[slot - NOW_DYN_FIRST] = role;
  Preferences p;
  if (p.begin("nodes", false)) { char key[4]; snprintf(key, sizeof(key), "r%u", slot); p.putUChar(key, role); p.end(); }
}
int16_t now_dyn_angle(uint8_t slot) { return now_dyn_present(slot) ? s_angle[slot - NOW_DYN_FIRST] : -1; }
bool now_dyn_free(uint8_t slot) { return now_dyn_present(slot) && s_free[slot - NOW_DYN_FIRST]; }

void now_send_servo_cfg(uint8_t slot, uint8_t pin, bool inv, uint8_t offAngle, uint8_t onAngle, uint8_t speed, uint8_t releaseDs, uint8_t minAngle, uint8_t maxAngle) {
  NowMsg a = {};
  a.type = NOW_SERVO_CFG; a.dev = slot; a.val = pin; a.st[0] = inv ? 1 : 0; a.st[1] = offAngle; a.st[2] = onAngle; a.st[3] = speed; a.seq = ++s_seq;
  NowMsg b = {};
  b.type = NOW_SERVO_CFG2; b.dev = slot; b.val = releaseDs; b.st[0] = minAngle; b.st[1] = maxAngle; b.st[2] = b.st[3] = NOW_UNKNOWN; b.seq = ++s_seq;
  for (int i = 0; i < 2; i++) { enqueue(a, linkOfDev(slot)); enqueue(b, linkOfDev(slot)); }   // paket kaybina karsi iki kez
}
void now_send_servo_goto(uint8_t slot, uint8_t angle, int speed) {
  NowMsg m = {};
  m.type = NOW_SERVO_GOTO; m.dev = slot; m.val = angle; m.st[0] = speed < 0 ? NOW_UNKNOWN : (uint8_t)speed; m.st[1] = m.st[2] = m.st[3] = NOW_UNKNOWN; m.seq = ++s_seq;
  enqueue(m, linkOfDev(slot));
}
void now_send_servo_free(uint8_t slot) {
  NowMsg m = {};
  m.type = NOW_SERVO_FREE; m.dev = slot; m.st[0] = m.st[1] = m.st[2] = m.st[3] = NOW_UNKNOWN; m.seq = ++s_seq;
  enqueue(m, linkOfDev(slot)); enqueue(m, linkOfDev(slot));
}
void now_send_pir_cfg(uint8_t slot, uint8_t pin, uint8_t holdS, uint8_t confirm10ms, uint8_t cooldownS, bool inv) {
  NowMsg m = {};
  m.type = NOW_PIR_CFG; m.dev = slot; m.val = pin; m.st[0] = holdS; m.st[1] = confirm10ms; m.st[2] = cooldownS; m.st[3] = inv ? 1 : 0; m.seq = ++s_seq;
  enqueue(m, linkOfDev(slot)); enqueue(m, linkOfDev(slot));
}

void now_send_cfg(uint8_t slot, uint8_t pin, bool inv) {
  NowMsg m = {};
  m.type = NOW_CFG; m.dev = slot; m.val = pin; m.st[0] = inv ? 1 : 0; m.st[1] = m.st[2] = m.st[3] = NOW_UNKNOWN; m.seq = ++s_seq;
  enqueue(m, linkOfDev(slot));
  enqueue(m, linkOfDev(slot));        // paket kaybina karsi iki kez
}

void now_forget(uint8_t slot) {
  NowMsg m = {};
  m.type = NOW_FORGET; m.dev = slot; m.st[0] = m.st[1] = m.st[2] = m.st[3] = NOW_UNKNOWN; m.seq = ++s_seq;
  for (int i = 0; i < 3; i++) enqueue(m, linkOfDev(slot));
  Preferences p;
  if (p.begin("nodes", false)) { char key[4]; snprintf(key, sizeof(key), "m%u", slot); p.remove(key); snprintf(key, sizeof(key), "r%u", slot); p.remove(key); p.end(); }
  if (slot >= NOW_DYN_FIRST && slot < NOW_DYN_FIRST + NOW_DYN_MAX) s_role[slot - NOW_DYN_FIRST] = ROLE_LIGHT;
  Serial.printf("[now] dugum unutuldu: yuva %u\n", slot);
}

void now_send_set(uint8_t dev, bool on) {
  NowMsg m = {};
  m.type = NOW_SET; m.dev = dev; m.val = on ? 1 : 0; m.seq = ++s_seq;
  enqueue(m, linkOfDev(dev));
}

void now_query(void) {
  for (uint8_t k = 0; k < LINKS; k++) {
    if (!s_link[k].sp) continue;
    NowMsg m = {};
    m.type = NOW_QUERY; m.seq = ++s_seq;
    enqueue(m, k);
  }
}

bool now_take_state(uint8_t st[4]) {
  if (!s_newState) return false;
  s_newState = false;
  for (int i = 0; i < 4; i++) st[i] = s_state[i];
  return true;
}

bool now_take_failed(void) {
  if (!s_failed) return false;
  s_failed = false;
  return true;
}

uint8_t now_link_down_mask(void) {
  const uint32_t now = millis();
  const Link &hub = s_link[0], &bed = s_link[1];
  uint8_t m = 0;
  // hub: hic gorulmediyse ya da 30 sn'dir sessizse (genel kopruda hub/yatak yok)
  if (hub.sp && (!hub.everSeen || now - hub.lastRxMs >= 30000)) m |= 1;
  // yatak: daha once goruldu ve 30 sn'dir sessiz, ya da son komut ulasamadi ve o zamandan beri ses yok
  if (bed.sp && bed.everSeen && now - bed.lastRxMs >= 30000) m |= 2;
  if (bed.lastFailMs > bed.lastRxMs && now - bed.lastFailMs < 120000) m |= 2;
  if (hub.lastFailMs > hub.lastRxMs && now - hub.lastFailMs < 120000) m |= 1;
  // eslesen dugumler: bit (2 + sira)
  for (int k = 2; k < LINKS; k++) {
    const Link &L = s_link[k];
    if (!L.sp) continue;
    const bool silent = L.everSeen ? (now - L.lastRxMs >= 30000) : (now - L.createdMs >= 45000);
    if (silent || (L.lastFailMs > L.lastRxMs && now - L.lastFailMs < 120000)) m |= (1 << k);
  }
  return m;
}

bool now_link_ok(void) {
  return now_link_down_mask() == 0;
}

void now_rssi_enable(bool on) {
  if (on == s_promisc) return;
  s_promisc = on;
  esp_wifi_set_promiscuous(on);
}

uint8_t now_channel(void) {
  uint8_t ch; wifi_second_chan_t c2;
  esp_wifi_get_channel(&ch, &c2);
  return ch;
}
