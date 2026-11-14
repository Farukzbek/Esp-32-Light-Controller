#include "HomeSpan.h"
#include "now_proto.h"
#include "now_link.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include <string.h>

// Kontrolcu iki cihazla konusur:
//   0 = masa dugumu:       DEV_MASA, DEV_LED
//   1 = yatak ESP'si:      DEV_YATAK
// SpanPoint::send() true donse bile karsi uygulama mesaji anlamis olmayabilir (802.11 donanim ACK'i
// yazilimdan bagimsiz gelir). Gercek onay, karsinin ayni seq ile dondurdugu STATE mesajidir.

#define LINKS 2
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
};

struct Out { NowMsg m; uint8_t link; };

static Link s_link[LINKS];

// --- Sinyal gucu (RSSI): promiscuous modda iki cihazin gonderdigi ESP-NOW cercevelerinden okunur ---
static bool s_promisc = false;      // promiscuous (RSSI) modu acik mi
static uint8_t s_mac[LINKS][6];
static volatile int8_t s_rssi[LINKS] = {0, 0};
static volatile uint32_t s_rssiMs[LINKS] = {0, 0};

static void promiscCb(void *buf, wifi_promiscuous_pkt_type_t) {
  const wifi_promiscuous_pkt_t *p = (const wifi_promiscuous_pkt_t *)buf;
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

static uint8_t linkOfDev(uint8_t dev) { return dev == DEV_YATAK ? 1 : 0; }

static void enqueue(const NowMsg &m, uint8_t link) {
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
    NowMsg in;
    while (L.sp->get(&in)) {
      if (in.type != NOW_STATE) continue;
      for (int i = 0; i < 4; i++) if (in.st[i] != NOW_UNKNOWN) s_state[i] = in.st[i];
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
      uint32_t t0 = millis();
      bool ok = L.sp->send(&o.m);
#if NOW_VERBOSE
      Serial.printf("[now:%s] gonderildi type=%d dev=%d val=%d seq=%u (radyo onayi: %s, %lu ms)\n", L.name, o.m.type, o.m.dev, o.m.val, o.m.seq, ok ? "ok" : "yok", (unsigned long)(millis() - t0));
#else
      (void)ok; (void)t0;
#endif
      if (o.m.type == NOW_SET) { L.pending = o.m; L.havePending = true; L.sentAt = millis(); L.tries = 1; }
    }

    // yanit gelmeyen komutlari yeniden dene
    for (int k = 0; k < LINKS; k++) {
      Link &L = s_link[k];
      if (!L.havePending || millis() - L.sentAt <= 350) continue;
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

void now_init(void) {
  SpanPoint::setPassword(NOW_PASSWORD);
  s_link[0].name = "hub";
  s_link[0].sp = new SpanPoint(NOW_HUB_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);
  s_link[1].name = "yatak";
  s_link[1].sp = new SpanPoint(NOW_BED_MAC, sizeof(NowMsg), sizeof(NowMsg), 4);
  s_q = xQueueCreate(12, sizeof(Out));
  parseMac(NOW_HUB_MAC, s_mac[0]);
  parseMac(NOW_BED_MAC, s_mac[1]);
  wifi_promiscuous_filter_t filt = {};
  filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(promiscCb);   // acmak icin now_rssi_enable(true)
  applyLock(NOW_CHANNEL);
  Serial.printf("[now] kumanda MAC = %s  masa = %s  yatak = %s  sabit kanal = %d\n", Network.macAddress().c_str(), NOW_HUB_MAC, NOW_BED_MAC, NOW_CHANNEL);
  xTaskCreate(nowTask, "now", 8192, nullptr, 1, nullptr);
}

void now_send_set(uint8_t dev, bool on) {
  NowMsg m = {};
  m.type = NOW_SET; m.dev = dev; m.val = on ? 1 : 0; m.seq = ++s_seq;
  enqueue(m, linkOfDev(dev));
}

void now_query(void) {
  for (uint8_t k = 0; k < LINKS; k++) {
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
  // hub: hic gorulmediyse ya da 30 sn'dir sessizse
  if (!hub.everSeen || now - hub.lastRxMs >= 30000) m |= 1;
  // yatak: daha once goruldu ve 30 sn'dir sessiz, ya da son komut ulasamadi ve o zamandan beri ses yok
  if (bed.everSeen && now - bed.lastRxMs >= 30000) m |= 2;
  if (bed.lastFailMs > bed.lastRxMs && now - bed.lastFailMs < 120000) m |= 2;
  if (hub.lastFailMs > hub.lastRxMs && now - hub.lastFailMs < 120000) m |= 1;
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
