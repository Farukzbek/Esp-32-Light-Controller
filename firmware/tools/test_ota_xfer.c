// Bilgisayarda calistir: c++ -std=c++17 -x c++ -I../shared test_ota_xfer.c -o /tmp/tox && /tmp/tox
// Kayip / cift / sirasiz teslim eden sahte bir ESP-NOW hattinda aktarim mantigini dener.
#include <stdio.h>
#include <string.h>
#include <vector>
#include <deque>
#include "ota_xfer.h"

static uint32_t rng_s = 1;
static uint32_t rnd() { rng_s = rng_s * 1664525u + 1013904223u; return rng_s >> 8; }
static bool chance(int percent) { return (int)(rnd() % 100) < percent; }

struct Frame { uint32_t off; uint16_t len; };
struct Ack { uint32_t expected; bool nak; };

struct Result { bool ok; uint32_t ticks, resends; };

// loss/dup/swap: yuzde. ackLoss: onay kaybi yuzde.
static Result run(uint32_t total, uint16_t chunk, uint8_t window, int loss, int dup, int swap, int ackLoss, int ackEvery) {
  std::vector<uint8_t> src(total), dst(total, 0);
  for (uint32_t i = 0; i < total; i++) src[i] = (uint8_t)(rnd());
  OtaTx tx; ota_tx_init(tx, total, chunk, window);
  OtaRx rx; ota_rx_init(rx, total);
  std::deque<Frame> fwd; std::deque<Ack> back;
  int sinceAck = 0, idle = 0;
  uint32_t ticks = 0;
  while (!(ota_tx_done(tx) && ota_rx_done(rx))) {
    if (++ticks > 2000000) return {false, ticks, tx.resends};
    // gonderen: pencere acik oldukca yolla
    uint32_t off; uint16_t len;
    while (ota_tx_next(tx, &off, &len)) fwd.push_back({off, len});
    // hat: kayip, cift, sirasiz
    std::deque<Frame> arr;
    while (!fwd.empty()) {
      Frame f = fwd.front(); fwd.pop_front();
      if (chance(loss)) continue;
      arr.push_back(f);
      if (chance(dup)) arr.push_back(f);
    }
    for (size_t i = 0; i + 1 < arr.size(); i++) if (chance(swap)) std::swap(arr[i], arr[i + 1]);
    // alan
    for (auto &f : arr) {
      OtaRxResult r = ota_rx_chunk(rx, f.off, f.len);
      if (r == OTA_RX_WRITE) {
        memcpy(&dst[f.off], &src[f.off], f.len);           // "flash'a yaz"
        if (++sinceAck >= ackEvery || ota_rx_done(rx)) { back.push_back({rx.expected, false}); sinceAck = 0; }
      } else if (r == OTA_RX_DUP) back.push_back({rx.expected, false});
      else if (r == OTA_RX_GAP) back.push_back({rx.expected, true});
      else return {false, ticks, tx.resends};              // BAD: bu testte hic olmamali
    }
    // onaylar
    bool got = false;
    while (!back.empty()) {
      Ack a = back.front(); back.pop_front();
      if (chance(ackLoss)) continue;
      ota_tx_on_ack(tx, a.expected, a.nak); got = true;
    }
    idle = got ? 0 : idle + 1;
    if (idle >= 3) { ota_tx_on_timeout(tx); idle = 0; }
  }
  return {dst == src, ticks, tx.resends};
}

int main() {
  int bad = 0;
  struct Case { const char *name; uint32_t total; uint16_t chunk; uint8_t win; int loss, dup, swap, ackLoss, ackEvery; };
  const Case cases[] = {
    {"temiz hat, kucuk",            1000,    180,  8,  0, 0, 0,  0, 4},
    {"tam parca siniri",             180,    180,  8,  0, 0, 0,  0, 4},
    {"1 bayt",                         1,    180,  8,  0, 0, 0,  0, 4},
    {"parca-1 bayt",                 179,    180,  8,  0, 0, 0,  0, 4},
    {"parca+1 bayt",                 181,    180,  8,  0, 0, 0,  0, 4},
    {"gercek boyut, temiz",      1340880,    180,  8,  0, 0, 0,  0, 4},
    {"%10 kayip",                 300000,    180,  8, 10, 0, 0,  0, 4},
    {"%10 kayip %10 onay kaybi",  300000,    180,  8, 10, 0, 0, 10, 4},
    {"cift ve sirasiz teslim",    300000,    180,  8,  3, 10, 10, 5, 4},
    {"cok kotu hat",              100000,    180,  8, 30, 10, 10, 30, 4},
    {"pencere 1 (dur-bekle)",      50000,    180,  1,  5, 0, 0,  5, 1},
    {"buyuk pencere",             300000,    180, 32,  5, 5, 5,  5, 16},
  };
  for (const Case &c : cases) {
    rng_s = 12345;
    Result r = run(c.total, c.chunk, c.win, c.loss, c.dup, c.swap, c.ackLoss, c.ackEvery);
    if (!r.ok) { printf("FAIL  %-28s (tick=%u)\n", c.name, r.ticks); bad++; }
    else printf("ok    %-28s tick=%-7u yeniden=%u\n", c.name, r.ticks, r.resends);
  }
  // gecersiz parcalar reddedilmeli
  OtaRx r; ota_rx_init(r, 1000);
  if (ota_rx_chunk(r, 0, 0) != OTA_RX_BAD) { puts("FAIL bos parca"); bad++; }
  if (ota_rx_chunk(r, 900, 200) != OTA_RX_BAD) { puts("FAIL sinir disi"); bad++; }
  if (ota_rx_chunk(r, 2000, 10) != OTA_RX_BAD) { puts("FAIL ofset sinir disi"); bad++; }
  ota_rx_chunk(r, 0, 100);
  if (ota_rx_chunk(r, 50, 100) != OTA_RX_BAD) { puts("FAIL ortusen parca"); bad++; }
  if (ota_rx_chunk(r, 0, 100) != OTA_RX_DUP) { puts("FAIL tekrar"); bad++; }
  if (ota_rx_chunk(r, 300, 100) != OTA_RX_GAP) { puts("FAIL bosluk"); bad++; }
  // gecersiz onay yok sayilmali
  OtaTx t; ota_tx_init(t, 1000, 180, 8);
  ota_tx_on_ack(t, 5000, false);
  if (t.acked != 0) { puts("FAIL gecersiz onay"); bad++; }
  if (bad == 0) puts("hepsi tamam");
  return bad;
}
