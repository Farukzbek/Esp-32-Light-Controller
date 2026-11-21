#pragma once
#include <stdint.h>

// Kapi otomasyonu: "hizli gir-cik". Bir kural calistiktan sonra kisa bir pencere (revSec sn) acilir; bu pencerede (ve aynı ziyaretteki
// art arda hareketleri saymamak icin en az minGapSec sn sonra) ikinci bir hareket gelirse kuralin eyleminin TERSI uygulanir
// (acildiysa kapat, kapandiysa ac, cevirdiyse tekrar cevir). Ornek: gece odaya girip bir sey alip 1 dakikadan kisa surede cikinca isiklar kapanir.
// Platform bagimsiz mantik: kapi dugumu (firmware/door) kullanir, bilgisayarda test edilir (tools/test_reenter.c).
struct ReenterState { bool armed = false; int8_t rule = -1; uint32_t armedAt = 0; bool locked = false; uint32_t lockUntil = 0, lockStart = 0; };

enum ReenterResult : uint8_t {
  RE_NONE = 0,      // pencere kapali ya da suresi doldu: kural normal calissin
  RE_SAME_VISIT,    // pencere acik ama cok erken (ayni ziyaret): ters calistirma, kurali da yeniden calistirma
  RE_REVERSE        // gir-cik algilandi: kuralin tersini uygula
};

inline void reenter_arm(ReenterState &s, int rule, uint32_t now) { s.armed = true; s.rule = (int8_t)rule; s.armedAt = now; }
inline void reenter_clear(ReenterState &s) { s.armed = false; s.rule = -1; }

// Yeni bir hareket geldi. revSec: pencere suresi (0 = ozellik kapali). Zaman asimini kendi temizler. s.rule, ters calistirilacak kuraldir.
inline ReenterResult reenter_check(ReenterState &s, uint32_t now, uint16_t revSec, uint16_t minGapSec) {
  if (!s.armed || revSec == 0) return RE_NONE;
  const uint32_t dt = now - s.armedAt;                       // millis tasmasina dayanikli
  if (dt > (uint32_t)revSec * 1000UL) { reenter_clear(s); return RE_NONE; }
  if (dt < (uint32_t)minGapSec * 1000UL) return RE_SAME_VISIT;
  return RE_REVERSE;
}

// Kural eyleminin tersi: 0 cevir -> 0, 1 ac -> 2 (kapat), 2 kapat -> 1 (ac)
inline uint8_t reenter_opposite(uint8_t action) { return action == 1 ? 2 : action == 2 ? 1 : action; }

// Sakinlesme: ters eylemden SONRA sensor cikan kisiyi gormeye devam eder (art arda hareketler). Bunlari yeni "giris" saymamak icin
// hareketler yok sayilir; her yeni hareket sureyi lockMs kadar uzatir AMA toplam yok sayma maxMs'yi asmaz (kapinin disinda oyalanan kisi
// sureyi sonsuza dek uzatip girisi geciktirmesin). Sonra kural normal calisir.
inline void reenter_lock(ReenterState &s, uint32_t now, uint32_t lockMs) { s.locked = true; s.lockStart = now; s.lockUntil = now + lockMs; }

// Yeni hareket geldi: sakinlesme suruyorsa true (hareket yok sayilmali) ve sure uzar (sinira kadar); bittiyse kilidi kaldirir ve false doner.
inline bool reenter_locked(ReenterState &s, uint32_t now, uint32_t lockMs, uint32_t maxMs) {
  if (!s.locked) return false;
  if ((int32_t)(now - s.lockUntil) < 0) {
    const uint32_t ext = now + lockMs, cap = s.lockStart + maxMs;
    s.lockUntil = ((int32_t)(ext - cap) > 0) ? cap : ext;
    return true;
  }
  s.locked = false;
  return false;
}

// Geri yukleme: cikista (isiklar kapatilirken) HANGI ISIKLARIN acik oldugu hatirlanir; geri girisle yalniz onlar yanar
// (ornek: yalnizca yatak aciksa cikinca yatak soner, girince yalniz yatak yanar; hepsi acikken hepsi). mask: bit = 1 << isik yuvasi.
struct RestoreState { bool valid = false; uint8_t mask = 0; uint32_t at = 0; };

inline void restore_set(RestoreState &s, uint8_t mask, uint32_t now) { s.valid = mask != 0; s.mask = mask; s.at = now; }
inline void restore_clear(RestoreState &s) { s.valid = false; s.mask = 0; }

// Giriste yakilacak isiklar: hatirlanan mask ile kuralin isiklarinin kesisimi. Hatirlanan yoksa ya da maxMs'den eskiyse 0 doner
// (cagiran kuralin tum isiklarini yakar: ornegin sabah cikip aksam donunce kural normal calissin). Kullanilinca hatirlanan silinir.
inline uint8_t restore_take(RestoreState &s, uint8_t ruleLamps, uint32_t now, uint32_t maxMs) {
  if (!s.valid) return 0;
  const bool fresh = (now - s.at) <= maxMs;
  const uint8_t m = fresh ? (uint8_t)(s.mask & ruleLamps) : 0;
  restore_clear(s);
  return m;
}
