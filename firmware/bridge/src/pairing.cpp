#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_random.h>
#include "now_proto.h"
#include "pair_crypto.h"
#include "now_link.h"
#include "pairing.h"

// Eslesme penceresi. HomeSpan, tanimadigi bir MAC'ten gelen ESP-NOW paketlerini atar; bu yuzden yeni dugumun HELLO'sunu
// ham radyodan (promiscuous) dinleriz. BEACON ve ACCEPT ham esp_now_send ile gider (yalnizca pencere acikken).

#define PAIR_WINDOW_MS 120000
#define MAX_CAND 6

struct Cand { bool used; uint8_t mac[6]; char name[17]; uint8_t pub[32]; int8_t rssi; uint32_t lastMs; uint8_t kind; uint8_t chip, defPin, defInv; };

static Cand s_cand[MAX_CAND];
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_open = false;
static uint32_t s_openedMs = 0, s_lastBeacon = 0;
static bool s_bcastPeer = false;

static String macToStr(const uint8_t m[6]) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}
static bool strToMac(const String &s, uint8_t out[6]) {
  unsigned v[6];
  if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)v[i];
  return true;
}

// Promiscuous geri cagrimi (WiFi gorevi): hizli ve kilitli kal. ESP-NOW cercevesi = eylem cercevesi:
//   24 bayt 802.11 baslik | 127 | OUI 18:FE:34 | 4 rastgele | 0xDD, uzunluk, OUI 18:FE:34, 0x04, surum | veri
void pair_sniff(const wifi_promiscuous_pkt_t *p) {
  if (!s_open) return;
  const uint8_t *d = p->payload;
  const int len = p->rx_ctrl.sig_len;
  if (len < 39 + (int)sizeof(PairHello)) return;
  if (d[0] != 0xD0) return;
  if (d[24] != 127 || d[25] != 0x18 || d[26] != 0xFE || d[27] != 0x34) return;
  if (d[32] != 221 || d[34] != 0x18 || d[35] != 0xFE || d[36] != 0x34 || d[37] != 0x04) return;
  if (d[33] - 5 != (int)sizeof(PairHello)) return;
  const PairHello *h = (const PairHello *)(d + 39);
  if (!pair_magic_ok(h->magic) || h->type != PAIR_HELLO || h->ver != PAIR_VERSION) return;
  const uint8_t *src = d + 10;
  portENTER_CRITICAL_ISR(&s_mux);
  Cand *slot = nullptr;
  for (int i = 0; i < MAX_CAND; i++) if (s_cand[i].used && memcmp(s_cand[i].mac, src, 6) == 0) { slot = &s_cand[i]; break; }
  if (!slot) for (int i = 0; i < MAX_CAND; i++) if (!s_cand[i].used) { slot = &s_cand[i]; break; }
  if (slot) {
    slot->used = true;
    memcpy(slot->mac, src, 6);
    memcpy(slot->name, h->name, 16); slot->name[16] = 0;
    memcpy(slot->pub, h->pub, 32);
    slot->kind = h->kind;
    slot->chip = h->chip; slot->defPin = h->defPin; slot->defInv = h->defInv;
    slot->rssi = p->rx_ctrl.rssi;
    slot->lastMs = millis();
  }
  portEXIT_CRITICAL_ISR(&s_mux);
}

static void ensureBroadcastPeer() {
  if (s_bcastPeer) return;
  esp_now_peer_info_t pi = {};
  memset(pi.peer_addr, 0xFF, 6);
  pi.channel = 0; pi.ifidx = WIFI_IF_STA; pi.encrypt = false;
  esp_now_add_peer(&pi);          // zaten varsa hata doner, sorun degil
  s_bcastPeer = true;
}

void pair_start() {
  portENTER_CRITICAL(&s_mux);
  memset(s_cand, 0, sizeof(s_cand));
  portEXIT_CRITICAL(&s_mux);
  ensureBroadcastPeer();
  s_openedMs = millis();
  s_lastBeacon = 0;
  s_open = true;
  now_rssi_enable(true);          // promiscuous: HELLO'lari duymak icin
  Serial.println("[pair] eslesme penceresi acildi");
}

void pair_stop() {
  if (!s_open) return;
  s_open = false;
  now_rssi_enable(false);
  Serial.println("[pair] eslesme penceresi kapandi");
}

bool pair_open() { return s_open; }

void pair_loop() {
  if (!s_open) return;
  if (millis() - s_openedMs > PAIR_WINDOW_MS) { pair_stop(); return; }
  if (millis() - s_lastBeacon >= 300) {
    s_lastBeacon = millis();
    PairBeacon b = {{PAIR_MAGIC0, PAIR_MAGIC1}, PAIR_BEACON, PAIR_VERSION};
    const uint8_t bc[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(bc, (const uint8_t *)&b, sizeof(b));
  }
}

String pair_json() {
  // Adaylari kilit altinda yalnizca KOPYALA; String islemleri (bellek ayirma) kilidin disinda yapilir.
  Cand snap[MAX_CAND];
  portENTER_CRITICAL(&s_mux);
  memcpy(snap, s_cand, sizeof(snap));
  portEXIT_CRITICAL(&s_mux);

  String o = "{\"open\":";
  o += s_open ? "true" : "false";
  const long left = s_open ? (long)((PAIR_WINDOW_MS - (millis() - s_openedMs)) / 1000) : 0;
  o += ",\"secondsLeft\":" + String(left < 0 ? 0 : left) + ",\"found\":[";
  bool first = true;
  for (int i = 0; i < MAX_CAND; i++) {
    const Cand &c = snap[i];
    if (!c.used || millis() - c.lastMs > 6000) continue;    // 6 sn'dir HELLO yok: dugum gitti
    String nm = String(c.name);
    nm.replace("\"", ""); nm.replace("\\", "");
    if (!first) o += ',';
    first = false;
    o += "{\"mac\":\"" + macToStr(c.mac) + "\",\"name\":\"" + nm + "\",\"rssi\":" + String((int)c.rssi) +
         ",\"chip\":" + String(c.chip) + ",\"pin\":" + String(c.defPin) + ",\"inv\":" + String(c.defInv) + "}";
  }
  o += "]}";
  return o;
}

bool pair_accept(const String &macStr, uint8_t slot, int pin, int inv, uint8_t role, String &err) {
  uint8_t mac[6];
  if (!strToMac(macStr, mac)) { err = "mac"; return false; }
  if (slot < NOW_DYN_FIRST || slot >= NOW_DYN_FIRST + NOW_DYN_MAX) { err = "yuva"; return false; }
  if (!s_open) { err = "pencere kapali"; return false; }

  Cand cand = {};
  bool found = false;
  portENTER_CRITICAL(&s_mux);
  for (int i = 0; i < MAX_CAND; i++)
    if (s_cand[i].used && memcmp(s_cand[i].mac, mac, 6) == 0 && millis() - s_cand[i].lastMs <= 8000) { cand = s_cand[i]; found = true; }
  portEXIT_CRITICAL(&s_mux);
  if (!found) { err = "cihaz gorunmuyor"; return false; }

  uint8_t bridgeMac[6];
  WiFi.macAddress(bridgeMac);
  uint8_t sec[32], pub[32], shared[32], key[32], iv[12];
  esp_fill_random(sec, 32);
  esp_fill_random(iv, 12);
  x25519_public(pub, sec);
  x25519_shared(shared, sec, cand.pub);
  pair_derive_key(key, shared, mac, bridgeMac);

  uint8_t ch = 0; wifi_second_chan_t c2;
  esp_wifi_get_channel(&ch, &c2);
  PairSecret ps = {};
  strncpy(ps.pass, now_net_key().c_str(), sizeof(ps.pass) - 1);
  ps.channel = ch;
  ps.slot = slot;
  ps.pin = (pin >= 0 && pin <= 60) ? (uint8_t)pin : cand.defPin;          // verilmediyse dugumun varsayilani
  ps.flags = ((inv >= 0 ? inv : cand.defInv) ? 1 : 0);
  ps.reserved[0] = role;                                                  // ROLE_LIGHT / ROLE_SERVO / ROLE_PIR

  PairAccept pa = {};
  pa.magic[0] = PAIR_MAGIC0; pa.magic[1] = PAIR_MAGIC1; pa.type = PAIR_ACCEPT; pa.ver = PAIR_VERSION;
  memcpy(pa.pub, pub, 32);
  memcpy(pa.iv, iv, 12);
  if (!pair_encrypt(key, iv, mac, bridgeMac, ps, pa.ct, pa.tag)) { err = "sifreleme"; return false; }

  // Dugume ham (sifresiz) gonder: dugum henuz parolayi bilmiyor. Birkac kez yolla (paket kaybi olabilir).
  esp_now_peer_info_t pi = {};
  memcpy(pi.peer_addr, mac, 6);
  pi.channel = 0; pi.ifidx = WIFI_IF_STA; pi.encrypt = false;
  esp_now_del_peer(mac);
  if (esp_now_add_peer(&pi) != ESP_OK) { err = "esle"; return false; }
  for (int i = 0; i < 4; i++) { esp_now_send(mac, (const uint8_t *)&pa, sizeof(pa)); delay(90); }
  esp_now_del_peer(mac);                  // kalici baglanti SpanPoint ile (sifreli) kurulacak

  if (!now_dyn_add(slot, mac)) { err = "kayit"; return false; }
  now_set_role(slot, role);
  // kullanilan aday listeden dussun
  portENTER_CRITICAL(&s_mux);
  for (int i = 0; i < MAX_CAND; i++) if (s_cand[i].used && memcmp(s_cand[i].mac, mac, 6) == 0) s_cand[i].used = false;
  portEXIT_CRITICAL(&s_mux);
  Serial.printf("[pair] %s yuva %u icin kabul edildi\n", macToStr(mac).c_str(), slot);
  return true;
}
