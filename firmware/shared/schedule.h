#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Zamanli otomasyon (kopruda calisir): "su saatte, su gunlerde su cihazlari ac / kapat / cevir / servoyu su acaya getir".
// Platform bagimsiz mantik: kopru (firmware/bridge) kullanir, bilgisayarda test edilir (tools/test_schedule.c).
// Plan: docs/plan-zamanli-otomasyon.md
//
// Kural metni (telefon -> kopru): "en:eylem:cihazlar:dakika:gunler:aci;..."
//   en       1 acik, 0 kapali
//   eylem    0 cevir, 1 ac, 2 kapat, 3 servo acisi (aci alani)
//   cihazlar bit = 1 << yuva (masa 0, yatak 1, led 2, d3..d7 = 3..7)
//   dakika   gun icindeki dakika, 0..1439 (yerel saat)
//   gunler   bit0 Pazartesi ... bit6 Pazar, 1..127
//   aci      0..180 (yalnizca eylem 3 icin anlamli)
#define SCHED_MAX 16

enum : uint8_t { SCHED_TOGGLE = 0, SCHED_ON = 1, SCHED_OFF = 2, SCHED_ANGLE = 3 };

struct SchedRule {
  bool en = true;
  uint8_t action = SCHED_ON;
  uint8_t devices = 1;
  uint16_t minute = 0;
  uint8_t days = 127;
  uint8_t angle = 90;
};

// "en:eylem:cihazlar:dakika:gunler:aci;..." -> kurallar. Gecersizse false (cikti degismez). n: bulunan kural sayisi.
inline bool sched_parse(const char *str, SchedRule *out, int maxRules, int *n) {
  SchedRule tmp[SCHED_MAX];
  int count = 0;
  const char *p = str;
  while (*p) {
    const char *end = strchr(p, ';');
    const size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len > 0) {
      if (count >= maxRules || count >= SCHED_MAX) return false;
      char item[64];
      if (len >= sizeof(item)) return false;
      memcpy(item, p, len); item[len] = 0;
      long v[6]; int k = 0; char *cur = item;
      while (k < 6) {
        char *colon = strchr(cur, ':');
        if (colon) *colon = 0;
        if (!*cur) return false;
        char *tail = nullptr; v[k++] = strtol(cur, &tail, 10);
        if (*tail) return false;
        if (colon && k == 6) return false;                 // 6'dan fazla alan
        if (!colon) break;
        cur = colon + 1;
      }
      if (k != 6 || v[1] < 0 || v[1] > 3 || v[2] < 1 || v[2] > 255 || v[3] < 0 || v[3] > 1439 || v[4] < 1 || v[4] > 127 || v[5] < 0 || v[5] > 180) return false;
      tmp[count++] = {v[0] != 0, (uint8_t)v[1], (uint8_t)v[2], (uint16_t)v[3], (uint8_t)v[4], (uint8_t)v[5]};
    }
    if (!end) break;
    p = end + 1;
  }
  for (int i = 0; i < count; i++) out[i] = tmp[i];
  *n = count;
  return true;
}

// Kurallari metne cevir (buf'a yazar, tasarsa false).
inline bool sched_to_string(const SchedRule *r, int n, char *buf, size_t size) {
  size_t used = 0; buf[0] = 0;
  for (int i = 0; i < n; i++) {
    char item[48];
    const int len = snprintf(item, sizeof(item), "%s%d:%u:%u:%u:%u:%u", i ? ";" : "", r[i].en ? 1 : 0, r[i].action, r[i].devices, r[i].minute, r[i].days, r[i].angle);
    if (len < 0 || used + len + 1 > size) return false;
    memcpy(buf + used, item, len + 1); used += len;
  }
  return true;
}

// Yerel takvim: epochDay = 1970-01-01'den beri gecen yerel gun sayisi, wday 0 = Pazartesi ... 6 = Pazar, minute 0..1439.
// 1970-01-01 Persembe: wday = (epochDay + 3) % 7.
inline int sched_wday(int32_t epochDay) { return (int)((epochDay % 7 + 7 + 3) % 7); }

// Calisma zamani takibi: her kural icin en son tetiklendigi "anahtar" (epochDay * 1440 + dakika). RAM'de tutulur.
struct SchedState { int64_t lastKey[SCHED_MAX]; SchedState() { for (int i = 0; i < SCHED_MAX; i++) lastKey[i] = -1; } };

// Su an tetiklenmesi gereken kurallarin maskesi (bit i = kural i). Her kural, hedef dakikasindan sonraki `catchUpMin` dakika icinde
// ve o gun icinde bir kez tetiklenir: catchUpMin = 0 -> yalnizca tam dakikada (kacirilan atlanir), > 0 -> kisa kesintiden sonra yakalar.
inline uint32_t sched_poll(const SchedRule *r, int n, SchedState &st, int32_t epochDay, uint16_t minute, int catchUpMin) {
  uint32_t due = 0;
  const int64_t nowKey = (int64_t)epochDay * 1440 + minute;
  for (int i = 0; i < n && i < SCHED_MAX; i++) {
    if (!r[i].en) continue;
    if (!((r[i].days >> sched_wday(epochDay)) & 1)) continue;
    const int64_t key = (int64_t)epochDay * 1440 + r[i].minute;
    if (nowKey < key || nowKey > key + catchUpMin) continue;
    if (st.lastKey[i] >= key) continue;                  // bu gunun bu tetigi zaten calisti
    st.lastKey[i] = key;
    due |= 1u << i;
  }
  return due;
}
