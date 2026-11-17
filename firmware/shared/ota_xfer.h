#pragma once
#include <stdint.h>

// Dugum yazilimi aktarimi (kopru -> dugum) icin PLATFORM BAGIMSIZ aktarim mantigi.
// ESP-NOW, HTTP ve flash bilgisi yok: yalnizca "hangi parca ne zaman gider, kayipta nereye geri sarilir".
// Hem kopru (gonderen) hem dugum (alan) bunu kullanir; bilgisayarda test edilir (tools/test_ota_xfer.c).
// Plan: docs/plan-dugum-ota.md

// ----------------------------------------------------------------------------- GONDEREN (kopru)
struct OtaTx {
  uint32_t total;     // imaj boyutu (bayt)
  uint16_t chunk;     // parca boyutu (bayt), tum parcalar bu boyutta, sonuncusu daha kisa olabilir
  uint8_t  window;    // ONAY beklemeden yollanabilecek en fazla parca
  uint32_t acked;     // alici "buraya kadar aldim" dedi (ofset)
  uint32_t next;      // yollanacak sonraki parcanin ofseti
  uint32_t resends;   // geri sarmayla yeniden yollanan parca sayisi (istatistik)
};

inline void ota_tx_init(OtaTx &t, uint32_t total, uint16_t chunk, uint8_t window) {
  t.total = total; t.chunk = chunk ? chunk : 1; t.window = window ? window : 1;
  t.acked = 0; t.next = 0; t.resends = 0;
}

// Yollanacak sonraki parca var mi? Pencere doluysa ya da imaj bittiyse false.
inline bool ota_tx_next(OtaTx &t, uint32_t *off, uint16_t *len) {
  if (t.next >= t.total) return false;
  if (t.next - t.acked >= (uint32_t)t.window * t.chunk) return false;
  const uint32_t left = t.total - t.next;
  *off = t.next;
  *len = left < t.chunk ? (uint16_t)left : t.chunk;
  t.next += *len;
  return true;
}

// Alicidan onay: "expected'a kadar aldim". nak == true ise alici bir bosluk gordu, expected'tan baslayarak yeniden yolla.
inline void ota_tx_on_ack(OtaTx &t, uint32_t expected, bool nak) {
  if (expected > t.total) return;                       // gecersiz onay
  if (expected > t.acked) t.acked = expected;
  if (t.next < t.acked) t.next = t.acked;               // onay, gonderenin onune gecmis olabilir (eski geri sarma sonrasi): ileri al
  // Eski (artik gecerli olmayan) NAK'a geri sarma: expected < acked ise yok say
  if (nak && expected >= t.acked && expected < t.next) {
    t.resends += (t.next - expected + t.chunk - 1) / t.chunk;
    t.next = expected;                                  // geri sar
  }
}

// Onay hic gelmedi (zaman asimi): bilinen son onaya geri sar.
inline void ota_tx_on_timeout(OtaTx &t) {
  if (t.next > t.acked) {
    t.resends += (t.next - t.acked + t.chunk - 1) / t.chunk;
    t.next = t.acked;
  }
}

inline bool ota_tx_done(const OtaTx &t) { return t.acked >= t.total; }

// ----------------------------------------------------------------------------- ALAN (dugum)
struct OtaRx {
  uint32_t total;     // BEGIN'de bildirilen imaj boyutu
  uint32_t expected;  // siradaki beklenen ofset (= su ana kadar sirayla yazilan bayt)
};

enum OtaRxResult : uint8_t {
  OTA_RX_WRITE = 0,   // beklenen parca: cagiran flash'a YAZSIN
  OTA_RX_DUP   = 1,   // zaten yazilmis (yeniden yollama): yazma, onay ver
  OTA_RX_GAP   = 2,   // oncesinde kayip var: yazma, NAK(expected) yolla
  OTA_RX_BAD   = 3    // gecersiz (sinir disi, bos, parcalar ortusuyor): iptal
};

inline void ota_rx_init(OtaRx &r, uint32_t total) { r.total = total; r.expected = 0; }

inline OtaRxResult ota_rx_chunk(OtaRx &r, uint32_t off, uint16_t len) {
  if (len == 0 || off > r.total || len > r.total - off) return OTA_RX_BAD;
  if (off == r.expected) { r.expected += len; return OTA_RX_WRITE; }
  if (off + len <= r.expected) return OTA_RX_DUP;
  if (off > r.expected) return OTA_RX_GAP;
  return OTA_RX_BAD;                                    // kismen ortusen parca
}

inline bool ota_rx_done(const OtaRx &r) { return r.expected >= r.total; }
